/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <paulp@go2net.com>
 *  Copyright (C) 1999-2004 Jon Nelson <jnelson@boa.org>
 *  Copyright (C) 1999-2000 Larry Doolittle <ldoolitt@boa.org>

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


#include "boa.h"
#include "escape.h"

/*
 * Name: req_write
 *
 * Description: Buffers data before sending to client.
 * Returns: -1 for error, otherwise how much is stored
 */

int req_write(request * req, const char *msg)
{
    unsigned int msg_len;

    msg_len = strlen(msg);

    if (!msg_len || req->status > DONE)
        return req->buffer_end;

    if (req->buffer_end + msg_len > sizeof(req->buffer)) {
        log_error_doc(req);
        fprintf(stderr, "There is not enough room in the buffer to"
                " copy %u bytes (%ld available). Shutting down connection.\n",
                msg_len,
                sizeof(req->buffer) - req->buffer_end);
#ifdef FASCIST_LOGGING
        *(req->buffer + req->buffer_end) = '\0';
        fprintf(stderr, "The request looks like this:\n%s\n",
                req->buffer);
#endif
        req->status = DEAD;
        return -1;
    }
    memcpy(req->buffer + req->buffer_end, msg, msg_len);
    req->buffer_end += msg_len;
    return req->buffer_end;
}

void reset_output_buffer(request * req)
{
    req->buffer_end = 0;
}

/*
 * Name: req_write_escape_http
 * Description: Buffers and "escapes" data before sending to client.
 *  as above, but translates as it copies, into a form suitably
 *  encoded for URLs in HTTP headers.
 * Returns: -1 for error, otherwise how much is stored
 */
int req_write_escape_http(request * req, const char *msg)
{
    char c, *dest;
    const char *inp;

    int left;
    inp = msg;
    dest = req->buffer + req->buffer_end;
    /* 3 is a guard band, since we don't check the destination pointer
     * in the middle of a transfer of up to 3 bytes */
    left = sizeof(req->buffer) - req->buffer_end;
    while ((c = *inp++) && left >= 3) {
        if (needs_escape((unsigned int) c)) {
            *dest++ = '%';
            *dest++ = INT_TO_HEX((c >> 4) & 0xf);
            *dest++ = INT_TO_HEX(c & 0xf);
            left -= 3;
        } else {
            *dest++ = c;
            left--;
        }
    }
    --inp;
    req->buffer_end = dest - req->buffer;

#ifdef TESTING
    if (left < 0) {
        log_error_time(); /* don't use log_error_doc here */
        fprintf(stderr, "Overflowed buffer space!\n");
        chdir("/tmp");
        abort();
    }
#endif

    if (*inp != '\0') {
        log_error_doc(req);
        fprintf(stderr, "Ran out of Buffer space! [req_write_escape_http]\n");
        req->status = DEAD;
        return -1;
    }
    return req->buffer_end;
}

/*
 * Name: req_write_escape_html
 * Description: Buffers and "escapes" data before sending to client.
 *  as above, but translates as it copies, into a form suitably
 *  encoded for HTML bodies.
 * Returns: -1 for error, otherwise how much is stored
 */
int req_write_escape_html(request * req, const char *msg)
{
    char c, *dest;
    const char *inp;
    int left;

    inp = msg;
    dest = req->buffer + req->buffer_end;
    /* 6 is a guard band, since we don't check the destination pointer
     * in the middle of a transfer of up to 6 bytes
     */
    left = sizeof(req->buffer) - req->buffer_end;
    while ((c = *inp++) && left >= 6) {
        switch (c) {
        case '>':
            *dest++ = '&';
            *dest++ = 'g';
            *dest++ = 't';
            *dest++ = ';';
            left -= 4;
            break;
        case '<':
            *dest++ = '&';
            *dest++ = 'l';
            *dest++ = 't';
            *dest++ = ';';
            left -= 4;
            break;
        case '&':
            *dest++ = '&';
            *dest++ = 'a';
            *dest++ = 'm';
            *dest++ = 'p';
            *dest++ = ';';
            left -= 5;
            break;
        case '\"':
            *dest++ = '&';
            *dest++ = 'q';
            *dest++ = 'u';
            *dest++ = 'o';
            *dest++ = 't';
            *dest++ = ';';
            left -= 6;
            break;
        default:
            *dest++ = c;
            left--;
        }
    }
    --inp;
    req->buffer_end = dest - req->buffer;

#ifdef TESTING
    if (left < 0) {
        log_error_time(); /* don't use log_error_doc here */
        fprintf(stderr, "Overflowed buffer space! [req_write_escape_html]\n");
        chdir("/tmp");
        abort();
    }
#endif

    if (*inp != '\0') {
        log_error_doc(req);
        fprintf(stderr, "Ran out of Buffer space (%d chars left)! "
                "[req_write_escape_html]\n", left);
        req->status = DEAD;
        return -1;
    }
    return req->buffer_end;
}


/*
 * Name: flush_req
 *
 * Description: Sends any backlogged buffer to client.
 *
 * Returns: -2 for error, -1 for blocked, otherwise how much is stored
 */

int req_flush(request * req)
{
    unsigned bytes_to_write;

    bytes_to_write = req->buffer_end - req->buffer_start;
    if (req->status > DONE)
        return -2;

    if (bytes_to_write) {
        int bytes_written;

        bytes_written = write(req->fd, req->buffer + req->buffer_start,
                              bytes_to_write);

        if (bytes_written < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return -1;      /* request blocked at the pipe level, but keep going */
            else {
                req->buffer_start = req->buffer_end = 0;
                /* OK to disable if your logs get too big */
#ifdef QUIET_DISCONNECT
                if (errno != ECONNRESET && errno != EPIPE)
#endif
                {
                    log_error_doc(req);
                    perror("buffer flush");
                }
                req->status = DEAD;
                req->buffer_end = 0;
                return -2;
            }
        }
#ifdef FASCIST_LOGGING
        log_error_time();
        fprintf(stderr, SOURCE_MARKER " - Wrote \"");
        fwrite(req->buffer + req->buffer_start, sizeof (char),
               bytes_written, stderr);
        fprintf(stderr, "\" (%d bytes)\n", bytes_written);
#endif
        req->buffer_start += bytes_written;
    }
    if (req->buffer_start == req->buffer_end)
        req->buffer_start = req->buffer_end = 0;
    return req->buffer_end;     /* successful */
}

