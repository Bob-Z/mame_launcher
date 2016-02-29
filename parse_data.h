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

#include "linked_list.h"
#include <expat.h>

typedef struct parse_data {
	XML_Parser parser;
	llist_t * root_node;
	int     decoding_string;
	char    decoded_string[1024];
	char ** xml_filter;
	llist_t * current;
} parse_data_t;

llist_t *LoadXML(const char * path, char ** filter);
void ElementStart(void *userData,const XML_Char *name,const XML_Char **atts);
void ElementEnd(void *userData,const XML_Char *name);
void CharacterHandler(void *userData,const XML_Char *s,int len);
