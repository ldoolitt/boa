/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <paulp@go2net.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@boa.org>
 *  Some changes Copyright (C) 1999 Jon Nelson <jnelson@boa.org>
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

/* $Id: log.c,v 1.36.2.20 2003/02/19 00:45:38 jnelson Exp $*/

#include "boa.h"

int cgi_log_fd;

FILE *fopen_gen_fd(char *spec, const char *mode);

FILE *fopen_gen_fd(char *spec, const char *mode)
{
    int fd;
    if (!spec || *spec == '\0')
        return NULL;
    fd = open_gen_fd(spec);
    if (fd == -1)
        return NULL;
    return fdopen(fd, mode);
}

/*
 * Name: open_logs
 *
 * Description: Opens access log, error log, and if specified, cgi log
 * Ties stderr to error log, except during cgi execution, at which
 * time cgi log is the stderr for cgis.
 *
 * Access log is line buffered, error log is not buffered.
 *
 */

void open_logs(void)
{
    int access_log;

    /* if error_log_name is set, dup2 stderr to it */
    /* otherwise, leave stderr alone */
    /* we don't want to tie stderr to /dev/null */
    if (error_log_name) {
        int error_log;

        /* open the log file */
        if (!(error_log = open_gen_fd(error_log_name))) {
            DIE("unable to open error log");
        }

        /* redirect stderr to error_log */
        if (dup2(error_log, STDERR_FILENO) == -1) {
            DIE("unable to dup2 the error log");
        }
        close(error_log);
    }

    if (access_log_name) {
        access_log = open_gen_fd(access_log_name);
    } else {
        access_log = open("/dev/null", 0);
    }
    if (access_log < 0) {
        DIE("unable to open access log");
    }

    if (dup2(access_log, STDOUT_FILENO) == -1) {
        DIE("can't dup2 /dev/null to STDOUT_FILENO");
    }
    close(access_log);

    if (cgi_log_name) {
        cgi_log_fd = open_gen_fd(cgi_log_name);
        if (cgi_log_fd == -1) {
            WARN("open cgi_log");
            free(cgi_log_name);
            cgi_log_name = NULL;
            cgi_log_fd = 0;
        } else {
            if (fcntl(cgi_log_fd, F_SETFD, 1) == -1) {
                WARN("unable to set close-on-exec flag for cgi_log");
                free(cgi_log_name);
                cgi_log_name = NULL;
                close(cgi_log_fd);
                cgi_log_fd = 0;
            }
        }
    }
#ifdef SETVBUF_REVERSED
    setvbuf(stderr, _IONBF, (char *) NULL, 0);
    setvbuf(stdout, _IOLBF, (char *) NULL, 0);
#else
    setvbuf(stderr, (char *) NULL, _IONBF, 0);
    setvbuf(stdout, (char *) NULL, _IOLBF, 0);
#endif
}

/*
 * Name: log_access
 *
 * Description: Writes log data to access_log.
 */

/* NOTES on the commonlog format:
 * Taken from notes on the NetBuddy program
 *  http://www.computer-dynamics.com/commonlog.html

 remotehost

 remotehost rfc931 authuser [date] "request" status bytes

 remotehost - IP of the client
 rfc931 - remote name of the user (always '-')
 authuser - username entered for authentication - almost always '-'
 [date] - the date in [08/Nov/1997:01:05:03 -0600] (with brackets) format
 "request" - literal request from the client (boa may clean this up,
   replacing control-characters with '_' perhaps - NOTE: not done)
 status - http status code
 bytes - number of bytes transferred

 boa appends:
   referer
   user-agent

 and may prepend (depending on configuration):
 virtualhost - the name or IP (depending on whether name-based
   virtualhosting is enabled) of the host the client accessed
*/

void log_access(request * req)
{
    if (!access_log_name)
        return;

    if (virtualhost) {
        printf("%s ", req->local_ip_addr);
    } else if (vhost_root) {
        printf("%s ", (req->host ? req->host : "(null)"));
    }
    printf("%s - - %s\"%s\" %d %ld \"%s\" \"%s\"\n",
           req->remote_ip_addr,
           get_commonlog_time(),
           req->logline,
           req->response_status,
           req->bytes_written,
           (req->header_referer ? req->header_referer : "-"),
           (req->header_user_agent ? req->header_user_agent : "-"));
}

/*
 * Name: log_error_doc
 *
 * Description: Logs the current time and transaction identification
 * to the stderr (the error log):
 * should always be followed by an fprintf to stderr
 *
 * Example output:
 [08/Nov/1997:01:05:03 -0600] request from 192.228.331.232 "GET /~joeblow/dir/ HTTP/1.0" ("/usr/user1/joeblow/public_html/dir/"): write: Broken pipe

 Apache uses:
 [Wed Oct 11 14:32:52 2000] [error] [client 127.0.0.1] client denied by server configuration: /export/home/live/ap/htdocs/test
 */

void log_error_doc(request * req)
{
    int errno_save = errno;

    if (virtualhost) {
        fprintf(stderr, "%s ", req->local_ip_addr);
    } else if (vhost_root) {
        fprintf(stderr, "%s ", (req->host ? req->host : "(null)"));
    }
    if (vhost_root) {
        fprintf(stderr, "%s - - %srequest [%s] \"%s\" (\"%s\"): ",
                req->remote_ip_addr,
                get_commonlog_time(),
                (req->header_host ? req->header_host : "(null)"),
                (req->logline ? req->logline : "(null)"),
                (req->pathname ? req->pathname : "(null)"));
    } else {
        fprintf(stderr, "%s - - %srequest \"%s\" (\"%s\"): ",
                req->remote_ip_addr,
                get_commonlog_time(),
                (req->logline ? req->logline : "(null)"),
                (req->pathname ? req->pathname : "(null)"));
    }

    errno = errno_save;
}

/*
 * Name: boa_perror
 *
 * Description: logs an error to user and error file both
 *
 */
void boa_perror(request * req, const char *message)
{
    log_error_doc(req);
    perror(message);            /* don't need to save errno because log_error_doc does */
    send_r_error(req);
}

/*
 * Name: log_error_time
 *
 * Description: Logs the current time to the stderr (the error log):
 * should always be followed by an fprintf to stderr
 */

void log_error_time(void)
{
    int errno_save = errno;
    fputs(get_commonlog_time(), stderr);
    errno = errno_save;
}

/*
 * Name: log_error
 *
 * Description: performs a log_error_time and writes a message to stderr
 *
 */

void log_error(const char *mesg)
{
    fprintf(stderr, "%s%s", get_commonlog_time(), mesg);
}

/*
 * Name: log_error_mesg
 *
 * Description: performs a log_error_time, writes the file and lineno
 * to stderr (saving errno), and then a perror with message
 *
 */

#ifdef HAVE_FUNC
void log_error_mesg(const char *file, int line, const char *func, const char *mesg)
{
    int errno_save = errno;
    fprintf(stderr, "%s%s:%d (%s) - ", get_commonlog_time(), file, line, func);
    errno = errno_save;
    perror(mesg);
    errno = errno_save;
}
#else
void log_error_mesg(const char *file, int line, const char *mesg)
{
    int errno_save = errno;
    fprintf(stderr, "%s%s:%d - ", get_commonlog_time(), file, line);
    errno = errno_save;
    perror(mesg);
    errno = errno_save;
}
#endif
