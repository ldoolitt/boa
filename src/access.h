/*
 *  Boa, an http server
 *  This file Copyright (C) 2002 Peter Korsgaard <jacmet@sunsite.dk>
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

/* $Id: access.h,v 1.1.2.2 2002/12/25 19:02:12 jnelson Exp $ */

#ifndef _ACCESS_H
#define _ACCESS_H

#define ACCESS_DENY 0
#define ACCESS_ALLOW 1

void access_shutdown(void);
void access_init(void);
void access_add(const char *pattern, const int type);
int access_allow(const char *file);

#endif                          /* _ACCESS_H */
