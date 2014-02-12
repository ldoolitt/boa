/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <paulp@go2net.com>
 *  Some changes Copyright (C) 1996 Charles F. Randall <crandall@goldsys.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@boa.org>
 *  Some changes Copyright (C) 1996-2002 Jon Nelson <jnelson@boa.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* $Id: boa.c,v 1.99 2002/03/24 22:22:26 jnelson Exp $*/

#include "boa.h"
#include <sys/resource.h>

/* globals */
int server_s;                   /* boa socket */
int backlog = SO_MAXCONN;
time_t start_time;

int sighup_flag = 0;            /* 1 => signal has happened, needs attention */
int sigchld_flag = 0;           /* 1 => signal has happened, needs attention */
int sigalrm_flag = 0;           /* 1 => signal has happened, needs attention */
int sigterm_flag = 0;           /* lame duck mode */
time_t current_time;
int max_fd = 0;
int pending_requests = 0;

/* static to boa.c */
static void fixup_server_root(void);
static void create_server_socket(void);
static void drop_privs(void);

int max_connections = 0;
static int sock_opt = 1;
static int do_fork = 1;
int devnullfd = -1;

int main(int argc, char **argv)
{
    int c;                      /* command line arg */
    struct rlimit rl;

    /* set umask to u+rw, u-x, go-rwx */
    umask(~0600);

    devnullfd = open("/dev/null", 0);

    /* make STDIN and STDOUT point to /dev/null */
    if (devnullfd == -1) {
        log_error_mesg(__FILE__, __LINE__, "can't open /dev/null");
        exit(errno);
    }

    if (dup2(devnullfd, STDIN_FILENO) == -1) {
        log_error_mesg(__FILE__, __LINE__,
                       "can't dup2 /dev/null to STDIN_FILENO");
        exit(errno);
    }

    if (dup2(devnullfd, STDOUT_FILENO) == -1) {
        log_error_mesg(__FILE__, __LINE__,
                       "can't dup2 /dev/null to STDOUT_FILENO");
        exit(errno);
    }

    /* but first, update timestamp, because log_error_time uses it */
    time(&current_time);

    while ((c = getopt(argc, argv, "c:r:d")) != -1) {
        switch (c) {
        case 'c':
            server_root = strdup(optarg);
            break;
        case 'r':
            if (chdir(optarg) == -1) {
                log_error_time();
                perror("chdir (to chroot)");
                exit(1);
            }
            if (chroot(optarg) == -1) {
                log_error_time();
                perror("chroot");
                exit(1);
            }
            if (chdir("/") == -1) {
                log_error_time();
                perror("chdir (after chroot)");
                exit(1);
            }
            break;
        case 'd':
            do_fork = 0;
            break;
        default:
            fprintf(stderr, "Usage: %s [-c serverroot] [-r chroot] [-d]\n", argv[0]);
            exit(1);
        }
    }

    fixup_server_root();
    read_config_files();
    open_logs();
    create_server_socket();
    init_signals();
    drop_privs();
    create_common_env();
    build_needs_escape();

    /* background ourself */
    if (do_fork) {
        if (fork())
            exit(0);
    }

    /* main loop */
    timestamp();

    getrlimit(RLIMIT_NOFILE, &rl);
    max_connections = rl.rlim_cur;

    status.requests = 0;
    status.errors = 0;

    start_time = current_time;
    select_loop(server_s);
    return 0;
}

static void create_server_socket(void)
{
    if ((server_s = socket(SERVER_AF, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        log_error_mesg(__FILE__, __LINE__, "unable to create socket");
        exit(errno);
    }

    /* server socket is nonblocking */
    if (set_nonblock_fd(server_s) == -1) {
        log_error_mesg(__FILE__, __LINE__,
                       "fcntl: unable to set server socket to nonblocking");
        exit(errno);
    }

    /* close server socket on exec so cgi's can't write to it */
    if (fcntl(server_s, F_SETFD, 1) == -1) {
        log_error_mesg(__FILE__, __LINE__,
                       "can't set close-on-exec on server socket!");
        exit(errno);
    }

    /* reuse socket addr */
    if ((setsockopt(server_s, SOL_SOCKET, SO_REUSEADDR, (void *) &sock_opt,
                    sizeof (sock_opt))) == -1) {
        log_error_mesg(__FILE__, __LINE__, "setsockopt");
        exit(errno);
    }

    /* internet family-specific code encapsulated in bind_server()  */
    if (bind_server(server_s, server_ip) == -1) {
        log_error_mesg(__FILE__, __LINE__, "unable to bind");
        exit(errno);
    }

    /* listen: large number just in case your kernel is nicely tweaked */
    if (listen(server_s, backlog) == -1) {
        log_error_mesg(__FILE__, __LINE__, "unable to listen");
        exit(errno);
    }
}

static void drop_privs(void)
{
    /* give away our privs if we can */
    if (getuid() == 0) {
        struct passwd *passwdbuf;
        passwdbuf = getpwuid(server_uid);
        if (passwdbuf == NULL) {
            log_error_mesg(__FILE__, __LINE__, "getpwuid");
            exit(errno);
        }
        if (initgroups(passwdbuf->pw_name, passwdbuf->pw_gid) == -1) {
            log_error_mesg(__FILE__, __LINE__, "initgroups");
            exit(errno);
        }
        if (setgid(server_gid) == -1) {
            log_error_mesg(__FILE__, __LINE__, "setgid");
            exit(errno);
        }
        if (setuid(server_uid) == -1) {
            log_error_mesg(__FILE__, __LINE__, "setuid");
            exit(errno);
        }
        /* test for failed-but-return-was-successful setuid
         * http://www.securityportal.com/list-archive/bugtraq/2000/Jun/0101.html
         */
        if (setuid(0) != -1) {
            log_error_mesg(__FILE__, __LINE__, "icky Linux kernel bug!");
            exit(1);
        }
    } else {
        if (server_gid || server_uid) {
            log_error_time();
            fprintf(stderr, "Warning: "
                    "Not running as root: no attempt to change"
                    " to uid %d gid %d\n", server_uid, server_gid);
        }
        server_gid = getgid();
        server_uid = getuid();
    }
}

/*
 * Name: fixup_server_root
 *
 * Description: Makes sure the server root is valid.
 *
 */

static void fixup_server_root()
{
    char *dirbuf;

    if (!server_root) {
#ifdef SERVER_ROOT
        server_root = strdup(SERVER_ROOT);
#else
        fputs("boa: don't know where server root is.  Please #define "
              "SERVER_ROOT in boa.h\n"
              "and recompile, or use the -c command line option to "
              "specify it.\n", stderr);
        exit(1);
#endif
    }

    if (chdir(server_root) == -1) {
        fprintf(stderr, "Could not chdir to \"%s\": aborting\n",
                server_root);
        exit(1);
    }

    dirbuf = normalize_path(server_root);
    free(server_root);
    server_root = dirbuf;
}

