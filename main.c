/*
   mame-launcher runs MAME emulator with random machines.
   Copyright (C) 2013-2017 carabobz@gmail.com

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

#define MAME_ROOT_NODE "mame"

#define PARAM_LISTXML "-listxml"
#define PARAM_GETSOFTLIST "-getsoftlist"
#define ENTRY_TYPE "machine"
#define SOFTWARELIST "softwarelist"

#define CONFIG_BASE_DIR "/.config"
#define CONFIG_DIR CONFIG_BASE_DIR "/mame_launcher"
#define WHITE_LIST CONFIG_DIR "/whitelist"
#define VERSION_FILENAME CONFIG_DIR "/version"
#define CACHE_LISTXML CONFIG_DIR "/listxml"
#define CACHE_GETSOFTLIST CONFIG_DIR "/getsoftlist"
#define AUTO_BLACK_LIST CONFIG_DIR "/auto_black_list"
#define AUTO_BLACK_SOFTLIST CONFIG_DIR "/auto_black_softlist"
#define DRIVER_BLACK_LIST CONFIG_DIR "/driver_black_list"
#define DESC_BLACK_LIST CONFIG_DIR "/desc_black_list"
#define CATEGORY_INI CONFIG_DIR "/category.ini"
#define OPTION_NO_SOUND " -nosound "
#define AUTO_MODE_OPTION "-nowindow -ui_active "
#define DURATION_OPTION "-str"

#define BUFFER_SIZE (10000)

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "linked_list.h"
#include "parse_data.h"
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include "misc.h"

char * working_dir=NULL;
char * binary=NULL;
char * whitelist_filename=NULL;
char * version_filename=NULL;
char * cache_listxml=NULL;
char * cache_getsoftlist=NULL;
char * auto_black_list_filename=NULL;
char * auto_black_softlist_filename=NULL;
char * driver_black_list_filename=NULL;
char * desc_black_list_filename=NULL;
char * category_ini_filename=NULL;

char ** category_ini_filter=NULL;

llist_t * listxml;
llist_t * softlists;
char * forced_list = NULL;

char option[BUFFER_SIZE] ;
char auto_mode_option[BUFFER_SIZE] ;
char command_line[BUFFER_SIZE];

int whitelist; /* whitelist file descriptor */
/* Names of drivers to skip when running in auto mode */
char ** auto_black_list = NULL;
/* Names of softlists to skip when running in auto mode */
char ** auto_black_softlist = NULL;
/* Names of drivers to skip when selecting a driver */
char ** driver_black_list = NULL;
/* Description string to black list (namely for fruit machine) */
char ** desc_black_list = NULL;

int automode = FALSE;
int chdmode = FALSE;
int preliminarymode = FALSE;
char* minyear = NULL;
char* min_players = NULL;
int whitelistmode = FALSE;
int coinonly = FALSE;
int gambling = FALSE;
int has_disk = FALSE;

struct termios orig_term_attr;
struct termios new_term_attr;

const char optstring[] = "?acpwl:y:nd:ogP:f:F:";
const struct option longopts[] =
        {{ "auto",no_argument,NULL,'a' },
        { "chd",no_argument,NULL,'c' },
        { "preliminary",no_argument,NULL,'p' },
        { "whitelist",no_argument,NULL,'w' },
        { "list",required_argument,NULL,'l' },
        { "year",required_argument,NULL,'y' },
        { "nosound",no_argument,NULL,'n' },
        { "duration",required_argument,NULL,'d' },
        { "coinonly",no_argument,NULL,'o' },
        { "gambling",no_argument,NULL,'g' },
        { "players",required_argument,NULL,'P' },
        { "filterin",required_argument,NULL,'F' },
        { "filterout",required_argument,NULL,'f' },
	{NULL,0,NULL,0}};


static char *filter[]= { "dipswitch", "dipvalue", "chip", "display", "sound", "configuration", "confsetting", "adjuster", "device", "instance", "extension", "slot", "slotoption", "ramoption", "publisher", "info", "sharedfeat", "manufacturer", "biosset", "device_ref", "sample", NULL };

static int is_machine_ok(llist_t * machine)
{
	llist_t * desc;
	llist_t * drv;
	llist_t * input;
	llist_t * disk;
	llist_t * control;
	llist_t * y;
	char * name;
	char * ismechanical;
	char * isdevice;
	char * runnable;
	char * coins;
	char * year;
	char * type;
	char * driver_status;
	char * players;
	int i;
	int is_OK = TRUE;

	name =find_attr(machine,"name");
	desc = find_first_node(machine,"description");

	// Driver black list
	i = 0;
	while( driver_black_list[i] != NULL ) {
		if(!strcmp(name,driver_black_list[i])){
			printf("    black-listed, skipping\n");
			is_OK = FALSE;
			break;
		}
		i++;
	}

	input = find_first_node(machine,"input");
	// coin only
	if(coinonly) {
		coins = find_attr(input,"coins");

		if( coins==NULL || coins[0]==0 ) {
			printf("    not coin operated, skipping\n");
			is_OK = FALSE;
		}
		else {
			printf("    coin operated\n");
		}
	}

	// gambling
	if(!gambling) {
		control = find_first_node(input,"control");
		while( control ) {
			type = find_attr(control,"type");
			if(!strcmp(type,"gambling")){
				printf("    gambling control, skipping\n");
				is_OK = FALSE;
				break;
			}
			control = find_next_node(control);
		}
	}

	// players
	if(min_players) {
		players = find_attr(input,"players");
		if( players == NULL ) {
			printf("    No players information, skipping\n");
			is_OK = FALSE;
		}
		else {
			if( strcmp(min_players,players) > 0 ) {
				printf("    Only %s players, skipping\n",players);
				is_OK = FALSE;
			}
			else {
				printf("    %s players allowed\n",players);
			}
		}
	}

	//minimal year
	y = find_first_node(machine,"year");
	if(y==NULL){
		if(minyear) {
			printf("    has no year information, skipping\n");
			is_OK = FALSE;
		}
	}
	else {
		year = y->data;
		if(year==NULL || year[0]==0){
			if(minyear) {
				printf("    has no year information, skipping\n");
				is_OK = FALSE;
			}
		}
		else {
			printf("    year is %s\n",year);
			if(minyear) {
				if( strcmp(year,minyear) < 0 ) {
					printf("    too old, skipping\n");
					is_OK = FALSE;
				}
			}
		}
	}

	// driver status
	drv = find_first_node(machine,"driver");
	driver_status = find_attr(drv,"status");
	if(driver_status && !strcmp(driver_status,"preliminary")) {
		if(!preliminarymode) {
			printf("    preliminary driver, skipping\n");
			is_OK = FALSE;
		}
		else {
			printf("    preliminary driver\n");
		}
	}

	// auto mode
	if( automode ) {
		i = 0;
		while(auto_black_list[i]!=NULL) {
			if(!strcmp(auto_black_list[i],name)) {
				printf("    black listed for auto-mode, skipping\n");
				is_OK = FALSE;
				break;
			}
			i++;
		}
	}

	// mechanical
	ismechanical = find_attr(machine,"ismechanical");
	if(!strcmp(ismechanical,"yes")) {
		printf("    mechanical, skipping\n");
		is_OK = FALSE;
	}

	// device
	isdevice = find_attr(machine,"isdevice");
	if(!strcmp(isdevice,"yes")) {
		printf("    device, skipping\n");
		is_OK = FALSE;
	}

	// runnable
	runnable = find_attr(machine,"runnable");
	if(!strcmp(runnable,"no")) {
		printf("    not runnable, skipping\n");
		is_OK = FALSE;
	}

	// Description black list
	i = 0;
	while( desc_black_list[i] != NULL ) {
		if(strstr(desc->data,desc_black_list[i])){
			printf("    black-listed by its description, skipping\n");
			is_OK = FALSE;
			break;
		}
		i++;
	}

	// category.ini list
	i = 0;
	while( desc_black_list[i] != NULL ) {
		if(strstr(desc->data,desc_black_list[i])){
			printf("    black-listed by its description, skipping\n");
			is_OK = FALSE;
			break;
		}
		i++;
	}

	//chd
	if( chdmode && !has_disk ) {
		disk = find_first_node(machine,"disk");
		if( disk == NULL ){
			printf("    not using disks, skipping\n");
			is_OK = FALSE;
		}
	}
	if( chdmode ) {
		printf("using disks\n");
	}

	return is_OK;
}

/* return a driver name which NEED TO BE FREED */
static char * select_random_driver(char * list, char * compatibility)
{
	llist_t * machine;
	llist_t * soft_list;
	llist_t * des;
	char * name;
	char * soft_name;
	char * compat;
	char ** driver = NULL;
	char ** desc = NULL;
	int driver_count = 0;
	int R;
	int i;
	char * selected_driver = NULL;

	machine = find_first_node(listxml,ENTRY_TYPE);
	do {
		soft_list = find_first_node(machine,"softwarelist");
		if(soft_list == NULL ) {
			continue;
		}
		do {
			soft_name = find_attr(soft_list,"name");
			if(!strcmp(soft_name,list)) {
				name =find_attr(machine,"name");
				des = find_first_node(machine,"description");
				printf(" - %s(%s)\n",des->data,name);

				if( ! is_machine_ok(machine) ) {
					continue;
				}

				// compatibility
				if(compatibility) {
					compat = find_attr(soft_list,"filter");
					if(compat) {
						if(strcmp(compat,compatibility)) {
							printf(" - driver %s(%s) is not compatible, skipping\n",des->data,name);
							continue;
						}
					}
				}

				driver_count++;
				driver = realloc(driver,sizeof(char*) * driver_count);
				desc = realloc(desc,sizeof(char*) * driver_count);
				driver[driver_count-1] = name;
				desc[driver_count-1] = des->data;
			}
		} while ( (soft_list=find_next_node(soft_list)) != NULL );

	} while ( (machine=find_next_node(machine)) != NULL );

	printf("%d compatible drivers for list %s",driver_count,list);
	if( driver_count == 0 ) {
		printf("\n\n");
		return NULL;
	}

	printf(" :");
	for(i=0;i<driver_count;i++) {
		printf(" %s",driver[i]);
	}
	printf("\n");

	R =rand()%driver_count;
	printf("\n%s\n",desc[R]);
	selected_driver = strdup(driver[R]);
	free(driver);
	free(desc);
	return selected_driver;
}

/* return 1 if system is launched in NON auto mode, 0 otherwise */
static int select_random_soft(int R)
{
	int count = 0;
	llist_t * machine;
	llist_t * soft_list;
	llist_t * software;
	llist_t * desc;
	llist_t * soft_desc;
	llist_t * part;
	llist_t * feature;
	llist_t * diskarea;
	char * name;
	char * soft_name;
	char * list_desc;
	char * selected_driver;
	char * compatibility;
	char * feat;
	char buf[BUFFER_SIZE];
	int i;

	has_disk = FALSE;

	// Try to find the requested number in listxml
	machine = find_first_node(listxml,ENTRY_TYPE);
	do {
		if(count == R) {
			name = find_attr(machine,"name");
			desc = find_first_node(machine,"description");
			printf(" - %s(%s)\n",desc->data,name);

			if( ! is_machine_ok(machine) ) {
				return 0;
			}

			sprintf(command_line,"%s",name);
			if(!automode) {
				sprintf(buf,"%s %s %s",binary,option,command_line);
				printf("Space to skip...\n");
				if( getchar() != 0x20 ) {
					printf("%s\n",buf);
					if(system(buf) == -1 ) {
						printf("Failed to run command %s\n",buf);
					}
					return 1;
				}
				return 0;
			}

			sleep(1);
			sprintf(buf,"%s %s %s %s",binary,option,auto_mode_option,command_line);
			printf("%s\n",buf);
			if(system(buf) == -1 ) {
				printf("Failed to run command %s\n",buf);
			}
			return 0;
		}
		count++;
	} while((machine=find_next_node(machine))!=NULL);

	// Try to find the requested number in softlists
	if( softlists != NULL ) {
		soft_list = find_first_node(softlists,SOFTWARELIST);
		do {
			software = find_first_node(soft_list,"software");
			do {
				if(count == R) {
					name = find_attr(soft_list,"name");

					if( automode ) {
						i = 0;
						while(auto_black_softlist[i]!=NULL) {
							if(!strcmp(auto_black_softlist[i],name)) {
								printf("softlist %s black listed for auto-mode\n",name);
								return 0;
							}
							i++;
						}
					}


					list_desc = find_attr(soft_list,"description");
					soft_name = find_attr(software,"name");
					soft_desc = find_first_node(software,"description");
#if 0
					supported = find_attr(software,"supported");
					if(!strcmp(supported,"no")) {
						printf("\n - %s (%s:%s) software not supported, skipping\n",soft_desc->data,name,soft_name);
						return 0;
					}
#endif

					/* Try to get compatiblity feature */
					compatibility = NULL;
					part = find_first_node(software,"part");
					while( part ) {
						feature = find_first_node(part,"feature");
						while( feature ) {
							feat = find_attr(feature,"name");
							if(!strcmp(feat,"compatibility")) {
								compatibility = find_attr(feature,"value");
								break;
							}
							feature = find_next_node(feature);
						}

						diskarea = find_first_node(part,"diskarea");
						if( diskarea ) {
							has_disk = TRUE;
						}
						part = find_next_node(part);
					}

					printf("Software list: %s (%s)\n",list_desc,name);
					selected_driver = select_random_driver(name,compatibility);
					if( selected_driver == NULL ) {
						return 0;
					}
					printf("%s\n",soft_desc->data);
					sprintf(command_line,"%s %s",selected_driver,soft_name);
					free(selected_driver);
					if(!automode) {
						sprintf(buf,"%s %s %s",binary,option,command_line);
						printf("Space to skip...\n");
						if( getchar() != 0x20 ) {
							printf("%s\n",buf);
							if(system(buf) == -1 ) {
								printf("Failed to run command %s\n",buf);
							}
							return 1;
						}
						return 0;
					}

					sleep(1);
					sprintf(buf,"%s %s %s %s",binary,option,auto_mode_option,command_line);
					printf("%s\n",buf);
					if(system(buf) == -1 ) {
						printf("Failed to run command %s\n",buf);
					}
					return 0;
				}
				count++;
			} while((software=find_next_node(software))!=NULL);
		} while((soft_list=find_next_node(soft_list))!=NULL);
	}

	printf("Error!!");
	return 0;
}

static void normal_mode()
{
	llist_t * machine;
	llist_t * soft_list;
	int entry_count;
	int softlist_count = -1;
	int software_count = -1;
	int R;
	int c;
	char * list_name = NULL;
	int forced_list_start = -1;
	int forced_list_count = 0;
	int count_forced_soft = 0;

	machine = find_first_node(listxml,ENTRY_TYPE);
	entry_count = 1;
	while((machine=find_next_node(machine))!=NULL) {
		entry_count++;
	}
	printf("%d entries\n",entry_count);

	if( softlists != NULL ) {
		softlist_count=0;
		software_count=0;
		machine = find_first_node(softlists,SOFTWARELIST);
		do {
			softlist_count ++;
			count_forced_soft = 0;

			if( forced_list ) {
				list_name = find_attr(machine,"name");
				if(!strcmp(forced_list,list_name)) {
					forced_list_start = software_count;
					count_forced_soft = 1;
				}
			}
			soft_list = find_first_node(machine,"software");
			do {
				software_count++;
				if(count_forced_soft) {
					forced_list_count++;
				}
			} while((soft_list=find_next_node(soft_list))!=NULL);
		} while((machine=find_next_node(machine))!=NULL);
	}
	printf("%d software lists\n",softlist_count);
	printf("%d softwares\n",software_count);

	if(forced_list) {
		printf("Forced list : %s\n",forced_list);
		if( forced_list_start == -1 ) {
			printf("%s list not found\n",forced_list);
			exit(-1);
		}
		if( forced_list_count == 0 ) {
			printf("No software found for list %s\n",forced_list);
			exit(-1);
		}
		printf("%d software in this list\n",forced_list_count);
	}


	while(1) {
		if ( forced_list ) {
			R = entry_count + forced_list_start + ( rand()%forced_list_count );
		}
		else {
			R = rand()%(software_count+entry_count);
		}

		printf("\n##############\n\n");
		if(select_random_soft(R)) {
			printf("[W]hite list software ?\n");
			c = getchar();
			if( c=='w' || c=='W' || c=='y' || c=='Y' ) {
				if( write(whitelist,"\n",strlen("\n")) == -1 ){
					printf("Failed to write to whitelist\n");
				}
				else {
					if( write(whitelist,command_line,strlen(command_line)) == -1 ) {
						printf("Failed to write to whitelist\n");
					}
				}
			}
		}
	}
}

static void * launch_load_listxml(void * arg)
{
	listxml = LoadXML(cache_listxml,filter);
	return NULL;
}

static void * launch_load_getsoftlist(void * arg)
{
	softlists = LoadXML(cache_getsoftlist,filter);
	return NULL;
}

static void whitelist_mode()
{
	FILE * whitelist = NULL;
	char ** list = NULL;
	size_t len;
	int count = 0;
	char buf[BUFFER_SIZE];
	char * entry = NULL;
	int R;
	char driver[BUFFER_SIZE];
	int i;

	/* Load list */
	whitelist = fopen(whitelist_filename,"r");
	while (  getline(&entry,&len,whitelist) !=  -1 ){
		count++;
		list=realloc(list,count*sizeof(char *));
		if(list == NULL) {
			printf("Memory error\n");
			exit(-1);
		}
		list[count-1] = entry;
		entry=NULL;
	}

	fclose(whitelist);

	while(1) {
		R =rand()%count;
		if( automode ) {
			sscanf(list[R],"%s",driver);
			i = 0;
			while(auto_black_list[i]!=NULL) {
				if(!strcmp(auto_black_list[i],driver)) {
					printf("%s black listed for auto-mode\n",driver);
					break;
				}
				i++;
			}
			if(auto_black_list[i] != NULL ) {
				continue;
			}
			sprintf(buf,"%s %s %s %s",binary,option,auto_mode_option,list[R]);
		}
		else {
			sprintf(buf,"%s %s %s",binary,option,list[R]);
		}
		printf("%s\n",buf);
		sleep(1);
		if(system(buf) == -1 ) {
			printf("Failed to run command %s\n",buf);
		}
	}
}

static void unset_terminal_mode(void)
{
	tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);
}

static char ** read_conf_file(const char * conf_filename)
{
	FILE * fp;
	int num;
	char buf[BUFFER_SIZE];
	char ** conf = NULL;

	conf = malloc(sizeof(char*));
	conf[0] = NULL;

	fp = fopen(conf_filename,"r");
	if( fp ){
		num = 0;
		while( fgets(buf, sizeof(buf), fp) ){
			if(buf[strlen(buf)-1]=='\n'){
				buf[strlen(buf)-1]=0;
			}
			num++;
			conf = realloc(conf,sizeof(char*)*(num+1));
			conf[num-1] = strdup(buf);
			conf[num] = NULL;
		}
		fclose( fp );
	}

	return conf;
}

static char* build_full_filename(const char * filename)
{
	char * tmp;
	char buf[BUFFER_SIZE];

	tmp = getenv("HOME");
	if(tmp == NULL) {
		printf("Please set HOME environnement variable");
		exit(-1);
	}

	strncpy(buf,tmp,BUFFER_SIZE);
	strncat(buf,filename,BUFFER_SIZE);
	return strdup(buf);
}

static void init()
{
	mkdir(CONFIG_BASE_DIR,0777);
	mkdir(CONFIG_DIR,0777);

	whitelist_filename=build_full_filename(WHITE_LIST);

	version_filename=build_full_filename(VERSION_FILENAME);

	cache_listxml=build_full_filename(CACHE_LISTXML);

	cache_getsoftlist=build_full_filename(CACHE_GETSOFTLIST);

	auto_black_list_filename=build_full_filename(AUTO_BLACK_LIST);
	auto_black_list= read_conf_file(auto_black_list_filename);

	auto_black_softlist_filename=build_full_filename(AUTO_BLACK_SOFTLIST);
	auto_black_softlist=read_conf_file(auto_black_softlist_filename);

	driver_black_list_filename=build_full_filename(DRIVER_BLACK_LIST);
	driver_black_list=read_conf_file(driver_black_list_filename);

	desc_black_list_filename=build_full_filename(DESC_BLACK_LIST);
	desc_black_list=read_conf_file(desc_black_list_filename);

	category_ini_filename=build_full_filename(CATEGORY_INI);

	printf("WORKING_DIR:         %s\n",working_dir);
	printf("BINARY:              %s\n",binary);
	printf("WHITE_LIST:          %s\n",whitelist_filename);
	printf("VERSION:             %s\n",version_filename);
	printf("CACHE_LISTXML:       %s\n",cache_listxml);
	printf("CACHE_GETSOFTLIST:   %s\n",cache_getsoftlist);
	if(auto_black_list) {
		printf("AUTO_BLACK_LIST:     %s\n",auto_black_list_filename);
	}
	if(auto_black_softlist) {
		printf("AUTO_BLACK_SOFTLIST: %s\n",auto_black_softlist_filename);
	}
	if(driver_black_list) {
		printf("DRIVER_BLACK_LIST:   %s\n",driver_black_list_filename);
	}
	if(desc_black_list) {
		printf("DESC_BLACK_LIST:     %s\n",desc_black_list_filename);
	}
	printf("\n\n");
}

static void get_binary_version(char * version)
{
	FILE *fpipe;
	char buffer[BUFFER_SIZE];

	sprintf(buffer,"%s -h",binary);
	fpipe = (FILE*)popen(buffer,"r");

	fgets( version, sizeof buffer, fpipe );

	pclose(fpipe);
}

static void get_cache_version(char * version)
{
	FILE *fversion;

	version[0]=0;
	fversion = fopen(version_filename,"r");
	if( fversion != NULL ) {
		fgets( version, BUFFER_SIZE, fversion );
		fclose(fversion);
	}
}

static void set_cache_version(char * version)
{
	FILE *fversion;

	fversion = fopen(version_filename,"w");
	fputs( version, fversion );
	fclose(fversion);
}

static int is_new_version()
{
	char buf_old[BUFFER_SIZE];
	char buf_new[BUFFER_SIZE];

	get_cache_version(buf_old);
	printf("Previous MAME version is: %s\n",buf_old);
	get_binary_version(buf_new);
	printf("Current MAME version is:  %s\n",buf_new);

	if( strncmp(buf_new,buf_old,sizeof buf_old) ){
		printf("New version available\n");
		return TRUE;
	}

	return FALSE;
}

void update_cache()
{
	char cmd[BUFFER_SIZE];
	FILE * fpipe;
	FILE * foutput;
	char * buffer;
	size_t bytes;

#define COPY_BUFFER_SIZE (1024*1024*100)
	buffer = malloc(COPY_BUFFER_SIZE);

	printf("Updating xml cache\n");
	sprintf(cmd,"%s %s",binary,PARAM_LISTXML);
	fpipe = (FILE*)popen(cmd,"r");
	foutput = fopen(cache_listxml,"w");
	while ((bytes = fread(buffer, 1, COPY_BUFFER_SIZE, fpipe)) > 0) {
			fwrite(buffer, 1, bytes, foutput);
	}
	fclose(foutput);
	pclose(fpipe);

	printf("Updating soft list cache\n");
	sprintf(cmd,"%s %s",binary,PARAM_GETSOFTLIST);
	fpipe = (FILE*)popen(cmd,"r");
	foutput = fopen(cache_getsoftlist,"w");
	while ((bytes = fread(buffer, 1, COPY_BUFFER_SIZE, fpipe)) > 0) {
			fwrite(buffer, 1, bytes, foutput);
	}
	fclose(foutput);
	pclose(fpipe);

	free(buffer);
}

/******************************************************************************/
void fill_filter(char * filter, int in_out)
{
	FILE * categoryini = NULL;
	size_t len = 0;
	int count = 0;
	char * entry = NULL;
	int category_found = 0;

	if( filter == NULL ) {
		return;
	}

	// Load list
	if( in_out == 0) {
		printf("Filter in loading\n");
	}
	else {
		printf("Filter out loading\n");
	}

	categoryini = fopen(category_ini_filename,"r");
	while (  getline(&entry,&len,categoryini) !=  -1 ){

		entry[strlen(entry)-1] = 0;
		if( entry[0] == ';' ) {
			free(entry);
			entry=NULL;
			len = 0;
			continue;
		}
			
		if( entry[0] == '[' ) {
			category_found = 0;
			if( in_out == 0 ) {
				if( strstr(entry,filter) != NULL ) {
					category_found = 1;
					printf("Adding in category %s\n", entry);
				}
			}
			else {
				if( strstr(entry,filter) == NULL ) {
					category_found = 1;
					printf("Adding out category %s\n", entry);
				}
			}

			free(entry);
			entry=NULL;
			len = 0;
			continue;
		}

		if( category_found == 1) {
			if( entry[1] != 0 ) {
				count++;
				category_ini_filter=realloc(category_ini_filter,count*sizeof(char *));
				category_ini_filter[count-1] = entry;
				//printf("Adding driver: %s\n",entry);
			}
		}
		else {
			free(entry);
		}

		entry=NULL;
		len = 0;
	}

	fclose(categoryini);

	count++;
	printf("%d drivers added\n\n", count);
	category_ini_filter=realloc(category_ini_filter,count*sizeof(char *));
	category_ini_filter[count-1] = NULL;
}

/******************************************************************************/
int main(int argc, char**argv)
{
	int opt_ret;
	char buffer[BUFFER_SIZE];
	int index;
	char * filter_in = NULL;
	char * filter_out = NULL;

	pthread_t thread_xml;
	pthread_t thread_softlist;

	void * ret;
	int duration = 60;

	srand ( time(NULL) );

	option[0] = 0;

	while((opt_ret = getopt_long(argc, argv, optstring, longopts, NULL))!=-1) {
		switch(opt_ret) {
			case 'a':
				automode = TRUE;
				break;
			case 'c':
				chdmode = TRUE;
				break;
			case 'p':
				preliminarymode = TRUE;
				break;
			case 'w':
				whitelistmode = TRUE;
				break;
			case 'o':
				coinonly = TRUE;
				break;
			case 'g':
				gambling = TRUE;
				break;
			case 'l':
				forced_list = optarg;
				break;
			case 'y':
				minyear = optarg;
				break;
			case 'n':
				strcat(option,OPTION_NO_SOUND);
				break;
			case 'd':
				duration = atoi(optarg);
				break;
			case 'P':
				min_players = optarg;
				break;
			case 'F':
				filter_in = optarg;
				break;
			case 'f':
				filter_out = optarg;
				break;
			default:
				printf("Usage:\n\n");
				printf("%s [OPTION] <Mame binary full path name>\n\n",argv[0]);
				printf("OPTION:\n");
				printf("-a : automatic mode\n");
				printf("-c : only disk based softwares (CHD)\n");
				printf("-d <seconds> : auto mode run duration\n");
				printf("-f <filter> : category.ini drivers within category name NOT containing <filter>\n");
				printf("-F <filter> : category.ini drivers within category name containing <filter>\n");
				printf("-g : allow gambling games (default is no gambling game)\n");
				printf("-l <list> : only use <list> software list\n");
				printf("-n : no sound\n");
				printf("-o : coin operated nachine only\n");
				printf("-p : allow preliminary drivers\n");
				printf("-P <num players>: only choose drivers allowing at least <num players> \n");
				printf("-w : use white list\n");
				printf("-y <4 digit year> : only choose drivers later than <year>\n");
				exit(0);
		}
	}

	for (index = optind; index < argc; index++) {
		binary = argv[index];
		break;
	}

	if( binary == NULL ) {
		printf("No binary full path name provided\n");
		exit(-1);
	}

	working_dir = strdup(binary);
	index = strlen(binary);
	while( index > 0 ) {
		if( working_dir[index] == '/' ) {
			working_dir[index] = 0;
			break;
		}
		index--;
	}

	if( index==0 ) {
		printf("Cannot find a path name in %s\n",binary);
		exit(-1);
	}

	sprintf(auto_mode_option," %s %s %d ",AUTO_MODE_OPTION,DURATION_OPTION,duration);

	init();

	fill_filter(filter_in,0);
	fill_filter(filter_out,1);

	if(chdir(working_dir) == -1) {
		printf("Failed to change to directory %s\n",working_dir);
		exit(-1);
	}

	if( whitelistmode ) {
		whitelist_mode();
	}

	if( is_new_version() ) {
		update_cache();
		get_binary_version(buffer);
		set_cache_version(buffer);
	}

	printf("Loading lists\n");
	pthread_create(&thread_xml,NULL,launch_load_listxml,NULL);
	pthread_create(&thread_softlist,NULL,launch_load_getsoftlist,NULL);
	pthread_join(thread_xml,&ret);
	pthread_join(thread_softlist,&ret);

	if(!automode) {
		/* set the terminal to raw mode */
		tcgetattr(fileno(stdin), &orig_term_attr);
		memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
		new_term_attr.c_lflag &= ~(ECHO|ICANON);
		tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

		atexit(unset_terminal_mode);
	}

	if(!automode) {
		whitelist = open(whitelist_filename,O_RDWR|O_CREAT,S_IRWXU|S_IRWXG|S_IROTH);
		if(whitelist == -1) {
			printf("Error openning whitelist\n");
			exit(1);
		}
		lseek(whitelist,0,SEEK_END);
	}
	normal_mode();

	return 0;
}

