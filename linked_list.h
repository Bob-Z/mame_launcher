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

#ifndef LINKED_LIST
#define LINKED_LIST

typedef struct llist {
	char * name;
	struct llist * next;
	struct llist * attr;
	struct llist * parent;
	struct llist * child;
	char * data;
} llist_t;

llist_t * new_llist();
void add_name(llist_t * list, char * name);
const char * get_name(llist_t * list);
void add_next(llist_t * llist, llist_t * next);
llist_t * get_next(llist_t * llist);
void add_attr(llist_t * llist, llist_t * attr);
llist_t * get_attr(llist_t * llist);
void add_parent(llist_t * llist, llist_t * parent);
llist_t * get_parent(llist_t * llist);
llist_t * add_child(llist_t * llist, llist_t * child);
llist_t * get_child(llist_t * llist);
void add_data(llist_t * llist, char * data);
char * get_data(llist_t * llist);
llist_t * find_node_by_attr(llist_t * list, const char * elem, const char * attr, const char * name);
char * find_attr(llist_t * l, const char * attr);
llist_t * find_first_node(llist_t * list, const char * elem);
llist_t * find_next_node(llist_t * list);
#endif
