/*
 *  Boa, an http server
 *  escape.c
 *  Copyright (C) 2001 Jon Nelson <jnelson@boa.org>
 *  Based on escape.pl, Copyright (C) 1996 Larry Doolittle <ldoolitt@boa.org>
 *  Copyright (C) 2001 Larry Doolittle <ldoolitt@boa.org>
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


/*
 unreserved = alnum | mark
 alnum = "0".."9" | "A".."Z" | "a".."z"
 mark = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
 noescape = unreserved | ":" | "@" | "&" | "=" | "+" | "$" | "," | "/"
 */

#ifdef TESTING
#include <stdio.h>
#include <stdlib.h>
#else
#include "boa.h"
#endif

#include "escape.h"

unsigned long
    _needs_escape[(NEEDS_ESCAPE_BITS + NEEDS_ESCAPE_WORD_LENGTH -
                   1) / NEEDS_ESCAPE_WORD_LENGTH];

void build_needs_escape(void)
{
    unsigned int a, b;
    const unsigned char special[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz" "0123456789" "-_.!~*'():@&=+$,/?";
    /* 21 Mar 2002 - jnelson - confirm with Apache 1.3.23 that '?'
     * is safe to leave unescaped.
     */
    unsigned short i, j;

    b = 1;
    for (a = 0; b != 0; a++)
        b = b << 1;
    /* I found $a bit positions available in an unsigned long. */
    if (a < NEEDS_ESCAPE_WORD_LENGTH) {
        fprintf(stderr,
                "NEEDS_ESCAPE_SHIFT configuration error -- "
                "%d should be <= log2(%d)\n", NEEDS_ESCAPE_SHIFT, a);
        exit(EXIT_FAILURE);
    } else if (a >= 2 * NEEDS_ESCAPE_WORD_LENGTH) {
        /* needs_escape_shift configuration suboptimal */
    } else {
        /* Ahh, just right! */ ;
    }
    memset(_needs_escape, ~0, sizeof (_needs_escape));
    for (i = 0; i < sizeof (special) - 1; ++i) {
        j = special[i];
        if (j >= NEEDS_ESCAPE_BITS) {
            /* warning: character $j will be needlessly escaped. */
        } else {
            _needs_escape[NEEDS_ESCAPE_INDEX(j)] &= ~NEEDS_ESCAPE_MASK(j);
        }
    }
}

/*
 * Name: html_escape_string
 */
char *html_escape_string(const char *inp, char *dest,
                         const unsigned int len)
{
    int max;
    char *buf;
    unsigned char c;

    max = len * 6;

    if (dest == NULL && max)
        dest = malloc(sizeof (unsigned char) * (max + 1));

    if (dest == NULL)
        return NULL;

    buf = dest;
    while ((c = *inp++)) {
        switch (c) {
        case '>':
            *dest++ = '&';
            *dest++ = 'g';
            *dest++ = 't';
            *dest++ = ';';
            break;
        case '<':
            *dest++ = '&';
            *dest++ = 'l';
            *dest++ = 't';
            *dest++ = ';';
            break;
        case '&':
            *dest++ = '&';
            *dest++ = 'a';
            *dest++ = 'm';
            *dest++ = 'p';
            *dest++ = ';';
            break;
        case '\"':
            *dest++ = '&';
            *dest++ = 'q';
            *dest++ = 'u';
            *dest++ = 'o';
            *dest++ = 't';
            *dest++ = ';';
            break;
        default:
            *dest++ = c;
        }
    }
    *dest = '\0';
    return buf;
}


/*
 * Name: http_escape_string
 *
 * Description: escapes the string inp.  Uses variable buf.  If buf is
 *  NULL when the program starts, it will attempt to dynamically allocate
 *  the space that it needs, otherwise it will assume that the user
 *  has already allocated enough space for the variable buf, which
 *  could be up to 3 times the size of inp.  If the routine dynamically
 *  allocates the space, the user is responsible for freeing it afterwords
 * Returns: NULL on error, pointer to string otherwise.
 */

char *http_escape_string(const char *inp, char *buf,
                         const unsigned int len)
{
    int max;
    char *index_c;
    unsigned char c;
    int found_a_colon = 0;

    max = len * 3;

    if (buf == NULL && max)
        buf = malloc(sizeof (unsigned char) * (max + 1));

    if (buf == NULL)
        return NULL;

    index_c = buf;
    while ((c = *inp++)) {
        if (c == ':' && !found_a_colon && index_c > buf) {
            found_a_colon = 1;
            memmove(buf + 2, buf, (index_c - buf));
            *buf = '.';
            *(buf + 1) = '/';
            index_c += 2;
            *index_c++ = ':';
        } else if (needs_escape((unsigned int) c) || c == '?') {
            *index_c++ = '%';
            *index_c++ = INT_TO_HEX((c >> 4) & 0xf);
            *index_c++ = INT_TO_HEX(c & 0xf);
        } else
            *index_c++ = c;
    }
    *index_c = '\0';

    return buf;
}



#ifdef TESTING
int main(void)
{
    int i;
    build_needs_escape();
    for (i = 0; i <= NEEDS_ESCAPE_BITS; ++i) {
        if (needs_escape(i)) {
            fprintf(stdout, "%3d needs escape.\n", i);
        }
    }
    return (0);
}
#endif
