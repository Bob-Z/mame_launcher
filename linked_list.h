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
