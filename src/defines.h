/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <paulp@go2net.com>
 *  Some changes Copyright (C) 1997 Jon Nelson <jnelson@boa.org>
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

/* $Id: defines.h,v 1.107.2.27 2003/02/18 16:21:35 jnelson Exp $*/

#ifndef _DEFINES_H
#define _DEFINES_H

/***** Change this, or use -c on the command line to specify it *****/

#ifndef SERVER_ROOT
#define SERVER_ROOT "/etc/boa"
#endif

/***** Change this via the CGIPath configuration value in boa.conf *****/
#define DEFAULT_PATH     "/bin:/usr/bin:/usr/local/bin"

/***** Change this via the DefaultVHost configuration directive in boa.conf *****/
#define DEFAULT_VHOST "default"
#define DEFAULT_CONFIG_FILE "boa.conf" /* locate me in the server root */

/***** Change this via the SinglePostLimit configuration value in boa.conf *****/
#define SINGLE_POST_LIMIT_DEFAULT               1024 * 1024 /* 1 MB */

/***** Various stuff that you may want to tweak, but probably shouldn't *****/

#define SOCKETBUF_SIZE                          32768
#define CLIENT_STREAM_SIZE                      8192
#define BUFFER_SIZE                             4096
#define MAX_HEADER_LENGTH			1024

#define MIME_HASHTABLE_SIZE			47
#define ALIAS_HASHTABLE_SIZE                    17
#define PASSWD_HASHTABLE_SIZE		        47

#define REQUEST_TIMEOUT				60

#define MIME_TYPES_DEFAULT                      "/etc/mime.types"
#define CGI_MIME_TYPE                           "application/x-httpd-cgi"

/***** CHANGE ANYTHING BELOW THIS LINE AT YOUR OWN PERIL *****/
/***** You will probably introduce buffer overruns unless you know
       what you are doing *****/

#define MAX_FILE_LENGTH				NAME_MAX
#define MAX_PATH_LENGTH				PATH_MAX

#ifdef ACCEPT_ON
#define MAX_ACCEPT_LENGTH MAX_HEADER_LENGTH
#else
#define MAX_ACCEPT_LENGTH 0
#endif

#ifndef SERVER_VERSION
#define SERVER_VERSION 				"Boa/0.94.14rc15"
#endif

#define CGI_VERSION				"CGI/1.1"

#ifdef USE_NCSA_CGI_ENV
#define COMMON_CGI_COUNT 8
#else
#define COMMON_CGI_COUNT 6
#endif

#define CGI_ENV_MAX     50
#define CGI_ARGC_MAX 128

#define SERVER_METHOD "http"

/*********** MMAP_LIST CONSTANTS ************************/
#define MMAP_LIST_SIZE 256
#define MMAP_LIST_MASK 255
#define MMAP_LIST_USE_MAX 128

#define MAX_FILE_MMAP 100 * 1024 /* 100K */

/*************** POLL / SELECT MACROS *******************/
#ifdef HAVE_POLL
#define BOA_READ (POLLIN|POLLPRI)
#define BOA_WRITE (POLLOUT)
#define BOA_FD_SET(req, thefd,where) { struct pollfd *my_pfd = &pfds[pfd_len]; req->pollfd_id = pfd_len++; my_pfd->fd = thefd; my_pfd->events = where; }
#define BOA_FD_CLR(req, fd, where) /* this doesn't do anything? */
#else                           /* SELECT */
#define BOA_READ (&block_read_fdset)
#define BOA_WRITE (&block_write_fdset)
#define BOA_FD_SET(req, fd, where) { FD_SET(fd, where); if (fd > max_fd) max_fd = fd; }
#define BOA_FD_CLR(req, fd, where) { FD_CLR(fd, where); }
#endif

/******** MACROS TO CHANGE BLOCK/NON-BLOCK **************/
/* If and when everyone has a modern gcc or other near-C99 compiler,
 * change these to static inline functions. Also note that since
 * we never fuss with O_APPEND append or O_ASYNC, we shouldn't have
 * to perform an extra system call to F_GETFL first.
 */
#ifdef BOA_USE_GETFL
#define set_block_fd(fd)    real_set_block_fd(fd)
#define set_nonblock_fd(fd) real_set_nonblock_fd(fd)
#else
#define set_block_fd(fd)    fcntl(fd, F_SETFL, 0)
#define set_nonblock_fd(fd) fcntl(fd, F_SETFL, NOBLOCK)
#endif

/***************** USEFUL MACROS ************************/

#define SQUASH_KA(req)	(req->keepalive=KA_STOPPED)

#ifdef HAVE_FUNC
#define WARN(mesg) log_error_mesg(__FILE__, __LINE__, __func__, mesg)
#else
#define WARN(mesg) log_error_mesg(__FILE__, __LINE__, mesg)
#endif
#define DIE(mesg) WARN(mesg), exit(1)

#endif
