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

llist_t *LoadXML(const char * path,parse_data_t * data, char ** filter);
void ElementStart(void *userData,const XML_Char *name,const XML_Char **atts);
void ElementEnd(void *userData,const XML_Char *name);
void CharacterHandler(void *userData,const XML_Char *s,int len);
