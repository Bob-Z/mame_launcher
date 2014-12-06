#include <expat.h>
#include <string.h>
#include "linked_list.h"
#include <stdio.h>
#include "parse_data.h"

/****************************************************************************

        Start Element

****************************************************************************/
void ElementStart(void *userData,const XML_Char *name,const XML_Char **atts)
{
	int i = 0;
	llist_t * list;
	parse_data_t * data = (parse_data_t *)userData;

	if( data->xml_filter ) {
		i=0;
		while(data->xml_filter[i] != NULL ) {
			if( !strcmp(name,data->xml_filter[i])) {
				return;
			}
			i++;
		}
	}

        /* Get the target node */
        if( data->current == NULL ) {
                list = new_llist();
                data->root_node = list; /*root node*/
        }
        else {
                list = (llist_t *)(data->current);
                list = add_child(list, new_llist());
        }

	/* add node name */
	add_name(list,strdup(name));

	/* add attributes name */
	llist_t * attr;
	i = 0;
	while( atts[i] != NULL ) {
		attr = new_llist();
		add_name(attr,strdup(atts[i++]));
		add_data(attr,strdup(atts[i++]));
		add_attr(list,attr);
	}

	/* Set current node as target node */
	data->current = list;
//	XML_SetUserData(data->parser,data);
}

/****************************************************************************

        End Element

****************************************************************************/
void ElementEnd(void *userData,const XML_Char *name)
{
	int i;
	parse_data_t * data = (parse_data_t *)userData;
	llist_t * list = data->current;

	if( data->xml_filter ) {
		i=0;
		while(data->xml_filter[i] != NULL ) {
			if( !strcmp(name,data->xml_filter[i])) {
				return;
			}
			i++;
		}
	}

	/* if we were decoding a string... */
	if( data->decoding_string ) {
		/* save it ... */
		add_data(list,strdup(data->decoded_string));
		/* and reset the string context for next string */
		data->decoding_string = 0;
		data->decoded_string[0] = 0;
	}

	data->current = get_parent(list);
//	XML_SetUserData(data->parser,data);
}

/****************************************************************************

        Charcter data handler

****************************************************************************/

void CharacterHandler(void *userData,const XML_Char *s,int len)
{
	parse_data_t * data = (parse_data_t *)userData;
	//llist_t * list = data->current;
	int i = 0;

	/* the same string may be splitted in several calls to CharacterHandler (namely
	when "non-XML-proof" charater are used in this string like < or > or &).
	So we have to save the state of the string being currently decoded until
	reaching an ElementEnd() */
	if( !data->decoding_string ) {
		data->decoding_string = 1;
		data->decoded_string[0]=0;
	}

	/* skip first useless bytes */
	while( len > 0 && ( s[i]=='\t' || s[i]=='\n') ) {
		i++;
		len--;
	}
	/* Remove useless trailing bytes */
	while( len > 0 && ( s[len-1]=='\t' || s[len-1]=='\n') ) {
		len--;
	}

	/* Add this poart of the string to the previous one (if there is a previous one)*/
	strncat(data->decoded_string, s+i, len);
}

/****************************************************************************

        LoadXML

****************************************************************************/

llist_t *LoadXML(const char * path,parse_data_t * data,char ** filter)
{
        /* Get MESS data*/
        FILE *fpipe;
        char buffer[1024];
        enum XML_Status status;

        if(path==NULL) {
                return NULL;
        }

	data->xml_filter = filter;

        /* Execute binary */
        sprintf(buffer,"%s",path);
        fpipe = (FILE*)popen(buffer,"r");
        if(fpipe==NULL) return NULL;

        /* XML parsing */
        data->parser=XML_ParserCreate("UTF-8");

        /* Element parsing */
        XML_SetStartElementHandler(data->parser,ElementStart);
        XML_SetEndElementHandler(data->parser,ElementEnd);

        /* character data parsing */
        XML_SetCharacterDataHandler(data->parser,CharacterHandler);

	
        XML_SetUserData(data->parser,data);

        status = XML_STATUS_OK;
        while ( fgets( buffer, sizeof buffer, fpipe) && status!=XML_STATUS_ERROR ) {
                status=XML_Parse(data->parser,buffer,strlen(buffer),0);
        }
        pclose(fpipe);

        if(status != XML_STATUS_OK) return NULL;

        return data->root_node;
}
