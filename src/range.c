/*
 *  Boa, an http server
 *  This file Copyright (C) 2000 by:
 *   Jon Nelson <jnelson@boa.org> and Larry Doolittle <ldoolitt@boa.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
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
 *  range.c  to test, build with
 *     gcc -Wall -DSTANDALONE_TEST -DFASCIST_LOGGING  range.c -o range
 *  and test with
 *     echo "bytes=0-10,15-9000,33-,-44,50-,30-50" | ./range 10000
 *  Parses client header information according to RFC2616, section 14.35.1
 */

#include "boa.h"

#define DEBUG if
#define DEBUG_RANGE 0

static void range_abort(request * req);
static void range_add(request * req, unsigned long start, unsigned long stop);

Range *range_pool = NULL;

void ranges_reset(request * req)
{
    Range *r = req->ranges;

    while (r) {
        Range *bob;

        bob = r->next;
        range_pool_push(r);
        r = bob;
    }
    req->ranges = NULL;
}

Range *range_pool_pop(void)
{
    Range *r;

    r = range_pool;
    if (r != NULL)
        range_pool = range_pool->next;
    else {
        /* have to make new range and return that */
        r = malloc(sizeof (Range));
        if (r == NULL)
            DIE("ran out of memory for new Range...");
    }
    r->start = 0;
    r->stop = 0;
    r->next = NULL;
    return r;
}

void range_pool_empty(void)
{
    while (range_pool != NULL) {
        Range *r = range_pool_pop();
        free(r);
    }
}

void range_pool_push(Range * r)
{
    r->next = range_pool;       /* whether or not range_pool is NULL */
    range_pool = r;
}

static void range_abort(request * req)
{
    /* free all ranges starting with head */
    ranges_reset(req);

    /* flag that we had an error, so no future ranges will be accepted */
    req->ranges = range_pool_pop();
    req->ranges->stop = ULONG_MAX;
}

static void range_add(request * req, unsigned long start, unsigned long stop)
{
    Range *prev;
    Range *r = range_pool_pop();

    DEBUG(DEBUG_RANGE) {
        fprintf(stderr, "range.c, add_range: got: %lu-%lu\n", start, stop);
    }

    for(prev = req->ranges;prev;prev = prev->next) {
        if (prev->next == NULL)
            break;
    }

    if (prev) {
        prev->next = r;
    } else {
        req->ranges = r;
    }

    r->start = start;
    r->stop = stop;
    req->numranges++;
}

/* parse_range converts the range string to a binary form _before_
 * we know the size of the file.  fixup_ranges touches up that
 * binary form _after_ we have req->filesize to work with.
 */
int ranges_fixup(request * req)
{
    /* loop through the ranges */
    Range *prev, *r;

    /* a -1 start means -xx
     * a -1 stop  means xx-
     */
    prev = NULL;
    r = req->ranges;
    req->ranges = NULL;
    while(r) {
        /* possible situations:
         * 1) start == -1 :: valid (so far)
         * 2) stop  == -1 :: valid (so far)
         * 3) both are -1 :: impossible
         * 4) start <= stop :: valid
         * 5) start > stop && start != -1 :: invalid
         */
        DEBUG(DEBUG_RANGE) {
            fprintf(stderr, "range.c: fixup_ranges: %lu-%lu\n", r->start, r->stop);
        }

        if ((long) r->start == -1) {
            r->start = req->filesize - r->stop;
            /* FIXME: Sanity check here for r->stop being "weird" */
            r->stop = req->filesize - 1;
        } else if (r->stop == -1) {
            /* r->start is *not* -1 */
            r->stop = req->filesize - 1;
        } else if (r->start > r->stop) {
            /* bad range . ignore */
            Range *temp;

            temp = r;
            if (prev)
                prev->next = r->next;
            r = r->next;
            range_pool_push(temp);
            DEBUG(DEBUG_RANGE) {
                fprintf(stderr, "end of range is invalid. skipping.\n");
            }
            continue;
        }

        DEBUG(DEBUG_RANGE) {
            fprintf(stderr, "start: %lu\tstop: %lu\n", r->start, r->stop);
        }
        if (r->start > req->filesize) { /* should we also check r->stop? */
            /* bad range . ignore */
            Range *temp;

            temp = r;
            if (prev)
                prev->next = r->next;
            r = r->next;
            range_pool_push(temp);
            DEBUG(DEBUG_RANGE) {
                fprintf(stderr, "start of range is invalid. skipping.\n");
            }
            continue;
        }

        if (r->stop > req->filesize - 1) {
            r->stop = req->filesize - 1;
        }

        if (prev == NULL)
            req->ranges = r;

        prev = r;
        r = r->next;
    }

    if (req->ranges == NULL) {
        /* bad range */
        send_r_invalid_range(req);
        return 0;
    }

    DEBUG(DEBUG_RANGE) {
        fprintf(stderr, "fixup_ranges returning 1\n");
    }
    return 1;
}

/*
 * Name: parse_range
 * Description: Takes a char* string and extracts Range information from it.
 * Expects a null-terminated string in a format similar to the following:
 *   "bytes=0-10,15-20000,33-,-44,50-,50-30"
 * Does not modify the input string.
 * Returns: 0 on error, 1 on success
 * Individual ranges are recorded using add_range.  "-1" is used
 * as a marker for "not given" (as with 33- and -44 above).
 */
int range_parse(request * req, const char *str)
{

#ifdef FASCIST_LOGGING
    fprintf(stderr, "parsing: %s\n", str);
#endif

    /* technically, this should be bytes (whitespace) = .... */
    if (strncasecmp(str, "bytes=", 6)) {
        /* error.  Doesn't start with 'bytes=' */
        fprintf(stderr, "range \"%s\" doesn't start with \"bytes=\"\n",
                str);
        return 0;
    }

    /* the only stuff in a Range field is
     *  whitespace, digits, ',' and '-'
     */
    str += 6;

    {
        /* States */
#define initial 0
#define startnum 1
#define gap1 2
#define gap2 3
#define stopnum 4
        int mode = initial;

        /* Character codes */
#define digit 0
#define white 1
#define comma 2
#define hyphen 3
#define null 4
#define other 5
        int ccode;
        unsigned long start = 0, stop = 0;

#define ACTMASK1 (0xE0)
#define PB  (0x20)              /* Push Beginning */
#define PE  (0x40)              /* Push End */
#define DB  (0x60)              /* Disable Beginning */
#define DE  (0x80)              /* Disable End */
#define ACTMASK2 (0x18)
#define AR  (0x10)              /* Abort Range */
#define SR  (0x18)              /* Submit Range */
#define STATEMASK (0x07)
        int stable[] = {
            /* digit      white       comma          hyphen   null   other */
            PB + startnum, 0 + initial, 0 + initial, DB + gap2, 0, AR, /* IN - initial */
            PB + startnum, 0 + gap1, AR, 0 + gap2, AR, AR, /* startnum */
            AR, 0 + gap1, AR, 0 + gap2, AR, AR, /* gap1 */
            PE + stopnum, 0 + gap2, DE + SR + initial, AR, DE + SR, AR, /* gap2 */
            PE + stopnum, SR + initial, SR + initial, AR, SR, AR
        };                      /* stopnum */

        int c;
        int fcode;

        for (;;) {
            c = *str++;
            /* There's probably a better way to code this character
             * code lookup, but this will do for now.  Cache pollution
             * is more important than raw speed. */
            switch (c) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                ccode = digit;
                break;
            case ' ':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
            case '\v':
                ccode = white;
                break;          /* whitespace not actually legal */
            case ',':
                ccode = comma;
                break;
            case '-':
                ccode = hyphen;
                break;
            case '\0':
                ccode = null;
                break;
            default:
                ccode = other;
            }

            fcode = stable[mode * 6 + ccode];
            if ((fcode & ACTMASK1) == PB)
                start = start * 10 + (c - '0');
            else if ((fcode & ACTMASK1) == DB)
                start = ULONG_MAX;
            else if ((fcode & ACTMASK1) == PE)
                stop = stop * 10 + (c - '0');
            else if ((fcode & ACTMASK1) == DE)
                stop = ULONG_MAX;
            if ((fcode & ACTMASK2) == AR) {
                range_abort(req);
                return 0;
            } else if ((fcode & ACTMASK2) == SR) {
                range_add(req, start, stop);
                start = 0;
                stop = 0;
            }
            if (ccode == null)
                return 1;
            mode = fcode & STATEMASK;
        }
    }
    return 1;
}

#ifdef STANDALONE_TEST
void send_r_invalid_range(request * req)
{
    fprintf(stderr, "send_r_invalid_range\n");
}

void log_error_mesg(char *file, int line, char *mesg, int die)
{
    fprintf(stderr, "%s:%d %s %d\n", file, line, mesg, die);
}

int main(int argc, char *argv[])
{
    request req;
    int c, fake_size = 10000;
    char buff[1024], *p;

    req.ranges = NULL;
    if (argc >= 2)
        fake_size = simple_itoa(argv[1]);
    while (fgets(buff, 1024, stdin)) {
        p = buff + strlen(buff) - 1;
        if (p >= buff && *p == '\n')
            *p = '\0';
        req.filesize = -666;
        c = parse_range(&req, buff);
        printf("parse_range returned %d\n", c);
        req.filesize = fake_size;
        c = fixup_ranges(&req);
        printf("fixup_ranges returned %d\n", c);
        reset_ranges(&req);
    }
    return 0;
}
#endif
