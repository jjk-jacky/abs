/**
 * based on inhibit.c   Copyright © 2007 Rafaël Carré           GPL 2+
 * based on xdg.c       Copyright (C) 2008 Rémi Denis-Courmont  LGPL 2.1+
 * */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <assert.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>

static int  Activate    (vlc_object_t *p_this);
static void Deactivate  (vlc_object_t *p_this);

static void Inhibit     (intf_thread_t *p_intf, bool suspend);

static int StateChange  (vlc_object_t *p_input, const char *var,
                         vlc_value_t prev, vlc_value_t value, void *data);
static int InputChange  (vlc_object_t *p_playlist, const char *var,
                         vlc_value_t prev, vlc_value_t value, void *data);
static void *Thread     (void *data);

struct intf_sys_t
{
    playlist_t         *p_playlist;
    vlc_object_t       *p_input;
    vlc_thread_t        thread;
    vlc_cond_t          update;
    vlc_cond_t          inactive;
    vlc_mutex_t         lock;
    posix_spawnattr_t   attr;
    bool                suspend;
    bool                suspended;
};

vlc_module_begin ()
    set_shortname (N_("inhibit-XDG"))
    set_description (N_("XDG screen saver inhibition"))
    set_capability ("interface", 0)
    set_callbacks (Activate, Deactivate)
vlc_module_end ()

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate (vlc_object_t *p_this)
{
    intf_thread_t   *p_intf = (intf_thread_t *) p_this;
    intf_sys_t      *p_sys;

    p_sys = p_intf->p_sys = (intf_sys_t *) calloc (1, sizeof (intf_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    p_sys->p_input = NULL;
    
    vlc_mutex_init (&p_sys->lock);
    vlc_cond_init (&p_sys->update);
    vlc_cond_init (&p_sys->inactive);
    posix_spawnattr_init (&p_sys->attr);
    /* Reset signal handlers to default and clear mask in the child process */
    {
        sigset_t set;

        sigemptyset (&set);
        posix_spawnattr_setsigmask (&p_sys->attr, &set);
        sigaddset (&set, SIGPIPE);
        posix_spawnattr_setsigdefault (&p_sys->attr, &set);
        posix_spawnattr_setflags (&p_sys->attr, POSIX_SPAWN_SETSIGDEF
                                              | POSIX_SPAWN_SETSIGMASK);
    }
    p_sys->suspend = false;
    p_sys->suspended = false;

    if (vlc_clone (&p_sys->thread, Thread, p_intf, VLC_THREAD_PRIORITY_LOW))
    {
        vlc_cond_destroy (&p_sys->inactive);
        vlc_cond_destroy (&p_sys->update);
        vlc_mutex_destroy (&p_sys->lock);
        free (p_sys);
        return VLC_ENOMEM;
    }
    p_sys->p_playlist = pl_Get (p_intf);
    var_AddCallback (p_sys->p_playlist, "item-current", InputChange, p_intf);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate (vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    var_DelCallback( p_sys->p_playlist, "item-current", InputChange, p_intf );

    if( p_sys->p_input ) /* Do delete "state" after "item-changed"! */
    {
        var_DelCallback( p_sys->p_input, "state", StateChange, p_intf );
        vlc_object_release( p_sys->p_input );
    }
    
    Inhibit (p_intf, false);

    /* Make sure xdg-screensaver is gone for good */
    vlc_mutex_lock (&p_sys->lock);
    while (p_sys->suspended)
        vlc_cond_wait (&p_sys->inactive, &p_sys->lock);
    vlc_mutex_unlock (&p_sys->lock);

    vlc_cancel (p_sys->thread);
    vlc_join (p_sys->thread, NULL);
    posix_spawnattr_destroy (&p_sys->attr);
    vlc_cond_destroy (&p_sys->inactive);
    vlc_cond_destroy (&p_sys->update);
    vlc_mutex_destroy (&p_sys->lock);

    free (p_sys);
}

static void Inhibit (intf_thread_t *p_intf, bool suspend)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    
    /* xdg-screensaver can take quite a while to start up (e.g. 1 second).
     * So we avoid _waiting_ for it unless we really need to (clean up). */
    vlc_mutex_lock (&p_sys->lock);
    p_sys->suspend = suspend;
    vlc_cond_signal (&p_sys->update);
    vlc_mutex_unlock (&p_sys->lock);
}

static int StateChange (vlc_object_t *p_input, const char *var,
                        vlc_value_t prev, vlc_value_t value, void *data)
{
    intf_thread_t *p_intf = data;
    const int old = prev.i_int, cur = value.i_int;

    if ((old == PLAYING_S) == (cur == PLAYING_S))
        return VLC_SUCCESS; /* No interesting change */

    if (cur == PLAYING_S)
    {
        Inhibit (p_intf, true);
    }
    else
    {
        Inhibit (p_intf, false);
    }

    (void)p_input; (void)var; (void)prev;
    return VLC_SUCCESS;
}

static int InputChange (vlc_object_t *p_playlist, const char *var,
                        vlc_value_t prev, vlc_value_t value, void *data)
{
    intf_thread_t *p_intf = data;
    intf_sys_t *p_sys = p_intf->p_sys;

    if (p_sys->p_input)
    {
        var_DelCallback (p_sys->p_input, "state", StateChange, p_intf);
        vlc_object_release (p_sys->p_input);
    }
    p_sys->p_input = VLC_OBJECT (playlist_CurrentInput (p_sys->p_playlist));
    if (p_sys->p_input)
    {
        Inhibit (p_intf, true);

        var_AddCallback (p_sys->p_input, "state", StateChange, p_intf);
    }

    (void)var; (void)prev; (void)value; (void)p_playlist;
    return VLC_SUCCESS;
}


extern char **environ;

VLC_NORETURN
static void *Thread (void *data)
{
    intf_thread_t *p_intf = data;
    intf_sys_t *p_sys = p_intf->p_sys;
    char id[] = "42";

    vlc_mutex_lock (&p_sys->lock);
    mutex_cleanup_push (&p_sys->lock);
    for (;;)
    {   /* TODO: detach the thread, so we don't need one at all time */
        while (p_sys->suspended == p_sys->suspend)
            vlc_cond_wait (&p_sys->update, &p_sys->lock);

        int canc = vlc_savecancel ();
        char *argv[4] = {
            (char *)"xdg-screensaver",
            (char *)(p_sys->suspend ? "suspend" : "resume"),
            id,
            NULL,
        };
        pid_t pid;

        vlc_mutex_unlock (&p_sys->lock);
        if (!posix_spawnp (&pid, "xdg-screensaver", NULL, &p_sys->attr,
                           argv, environ))
        {
            int status;

            msg_Dbg (p_intf, "started xdg-screensaver (PID = %d)", (int)pid);
            /* Wait for command to complete */
            while (waitpid (pid, &status, 0) == -1);
        }
        else/* We don't handle the error, but busy looping would be worse :( */
            msg_Warn (p_intf, "could not start xdg-screensaver");

        vlc_mutex_lock (&p_sys->lock);
        p_sys->suspended = p_sys->suspend;
        if (!p_sys->suspended)
            vlc_cond_signal (&p_sys->inactive);
        vlc_restorecancel (canc);
    }

    vlc_cleanup_pop ();
    assert (0);
}
