/*
   mame-launcher runs MAME emulator with random machines.
   Copyright (C) 2013-2015 carabobz@gmail.com

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <stdlib.h>
#include <string.h>

/******************************
 * Return the temporay directory path
 *
 * Result MUST be freed
 ******************************/
char * get_tmp_dir()
{
	char * tmp;

	tmp = getenv("TMP");
	if( tmp == NULL ) {
		tmp = getenv("TMPDIR");
		if( tmp == NULL ) {
			tmp = getenv("TEMP");
			if( tmp == NULL ) {
				tmp = getenv("TEMPDIR");
				if( tmp == NULL ) {
					tmp = "/tmp";
				}
			}
		}
	}

	return strdup(tmp);
}
