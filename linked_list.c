#include "linked_list.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

llist_t * new_llist()
{
	llist_t * new_list;

	new_list = malloc(sizeof(llist_t));
	if( new_list == NULL) return NULL;
	new_list->name = NULL;
	new_list->next = NULL;
	new_list->attr = NULL;
	new_list->parent = NULL;
	new_list->child = NULL;
	new_list->data = NULL;

	return new_list;
}

inline void add_name(llist_t * list, char * name)
{
	list->name = name;
}

inline const char * get_name(llist_t * list)
{
	return list->name;
}

void add_next(llist_t * list, llist_t * next)
{
	list->next = next;
	add_parent(next,get_parent(list));
}

inline llist_t * get_next(llist_t * list)
{
	return list->next;
}

/* add an attribute at the beginning of the attribute linked list of the given list*/
void add_attr(llist_t * list, llist_t * attr)
{
	llist_t * at;

	at = get_attr(list);

	list->attr = attr;
	add_parent(attr,list);

	if(at != NULL) {
		attr->next = at;
	}
}

inline llist_t * get_attr(llist_t * list)
{
	return list->attr;
}

inline void add_parent(llist_t * list, llist_t * parent)
{
	list->parent = parent;
}

inline llist_t * get_parent(llist_t * list)
{
	return list->parent;
}


/* add a child at the beginning of the children linked list of the given list*/
llist_t * add_child(llist_t * list, llist_t * child)
{
	llist_t * at;

	at = get_child(list);

	list->child = child;
	add_parent(child,list);

	if(at != NULL) {
		child->next = at;
	}

	return child;
}

inline llist_t * get_child(llist_t * list)
{
	return list->child;
}

inline void add_data(llist_t * list, char * data)
{
	list->data = data;
}

inline char * get_data(llist_t * list)
{
	if(list == NULL) return NULL;

	return list->data;
}
/*
char * find_elem_by_attr(llist_t * list, int depth, ...){
        int i;
        va_list ap;
	char * node_name;
	llist_t * l = list;

        va_start(ap, depth);
        for(i = 0; i < depth; i++) {
		node_name = va_arg(ap, char *);
		l = get_child(l);
		while( strcmp(get_name(l), node_name) != 0 ) {
			l = get_next(l);
			if(l == NULL) {
				return NULL;
			}
		}
        }
        va_end(ap);

        return get_data(l);
}
*/
llist_t * find_node_by_attr(llist_t * list, const char * elem, const char * attr, const char * name){
	llist_t * l;	
	llist_t * m;	

	l = get_child(list);
	do {
		if( strcmp(elem, get_name(l)) == 0 ) {
			m = get_attr(l);
			do {
				if( strcmp(attr, get_name(m)) == 0 ) {
					if( strcmp(name, get_data(m)) == 0 ) {
						return get_parent(m);
					}
				}
			} while( (m=get_next(m)) != NULL );
		}
	} while( (l=get_next(l)) != NULL );

	return NULL;	
}


char * find_attr(llist_t * l, const char * attr){
	llist_t * m;	

	if(l == NULL) return NULL;
	if(attr == NULL) return NULL;

	m = get_attr(l);
	do {
		if( strcmp(attr, get_name(m)) == 0 ) {
			return get_data(m);
		}
	} while( (m=get_next(m)) != NULL );

	return NULL;
}


llist_t * find_first_node(llist_t * list, const char * elem){
	llist_t * l;	

	l = get_child(list);
	do {
		if( strcmp(elem, get_name(l)) == 0 ) {
			return(l);
		}
	} while( (l=get_next(l)) != NULL );

	return NULL;	
}

llist_t * find_next_node(llist_t * list){
	llist_t * l;	

	l = list;
	while( (l=get_next(l)) != NULL ) {
		if( strcmp(get_name(list), get_name(l)) == 0 ) {
			return(l);
		}
	} 

	return NULL;	
}
