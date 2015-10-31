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

//#define system(x) puts(x)

#define MAME_ROOT_NODE "mame"

#define PARAM_LISTXML "-listxml"
#define PARAM_GETSOFTLIST "-getsoftlist"
#define ENTRY_TYPE "machine"
#define SOFTWARELIST "softwarelist"

#define WHITE_LIST "/.config/mame_launcher/whitelist"
#define MAX_DRIVER (10000)
#define OPTION_NO_SOUND " -nosound "
#define AUTO_MODE_OPTION "-nowindow"
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

char * tmp_dir=NULL;
char * working_dir=NULL;
char * binary=NULL;
char * roms_dir=NULL;
char * root_node=NULL;
char * white_list=NULL;

llist_t * listxml;
llist_t * softlist;
char **chd_list_dir = NULL;
char **chd_list_file = NULL;
int chd_count = 0;
char * forced_list = NULL;

char option[BUFFER_SIZE] ;
char auto_mode_option[BUFFER_SIZE] ;
char command_line[BUFFER_SIZE];

int whitelist; /* whitelist file descriptor */
/* Names of directories to skip during CHD scanning */
char * chd_black_list[] = { "pcecd", "cdi", "pippin", NULL} ;
/* Names of drivers to skip when running in auto mode */
char * auto_black_list[] = { "cdimono1", "cpc464", "cpc664", "cpc6128", "al520ex", "kccomp", "cpc6128f", "cpc6128s", NULL } ;
/* Names of softlist to skip when running in auto mode */
char * auto_black_softlist[] = { "tvc_flop", "ti99_cart", "bbca_cass", "bbcb_cass", "msx1_cass", "lviv", "kc_cass", "spc1000_cass", "sol20_cass", "mtx_cass", "dai_cass", "ep64_flop", "pet_cass", "orion_cass", "mz700_cass", "special_cass", "cbm2_flop", "cgenie_cass", "jupace_cass", NULL } ;
/* Names of drivers to skip when selecting a driver */
char * driver_black_list[] = { "kccomp", "al520ex", "cpc6128s", "cpc6128f", NULL };
/* Description string to black list (namely for fruit machine) */
char * desc_black_list[] = { "(MPU4)", "(Scorpion 4)", NULL };

int automode = 0;
int chdmode = 0;
int preliminarymode = 0;
int minyear = 0;
int update = 0;
int whitelistmode = 0;

struct termios orig_term_attr;
struct termios new_term_attr;

const char optstring[] = "?acpwul:y:nd:";
const struct option longopts[] =
        {{ "auto",no_argument,NULL,'a' },
        { "chd",no_argument,NULL,'c' },
        { "preliminary",no_argument,NULL,'p' },
        { "update",no_argument,NULL,'u' },
        { "whitelist",no_argument,NULL,'w' },
        { "list",required_argument,NULL,'l' },
        { "year",required_argument,NULL,'y' },
        { "nosound",no_argument,NULL,'n' },
        { "duration",required_argument,NULL,'d' },
	{NULL,0,NULL,0}};


static char *filter[]= { "dipswitch", "dipvalue", "chip", "display", "sound", "input", "control", "configuration", "confsetting", "adjuster", "device", "instance", "extension", "slot", "slotoption", "ramoption", "publisher", "info", "sharedfeat", "manufacturer", "biosset", "device_ref", "sample", NULL };

char * select_random_driver(char * list, char * compatibility)
{
	llist_t * current;
	llist_t * soft_list;
	llist_t * des;
	llist_t * drv;
	llist_t * y;
	char * name;
	char * soft_name;
	char * driver_status;
	char * year;
	char * driver[MAX_DRIVER];
	char * desc[MAX_DRIVER];
	char * compat;
	int driver_count = 0;
	int R;
	int i;

	current = find_first_node(listxml,ENTRY_TYPE);
	do {
		soft_list = find_first_node(current,"softwarelist");
		if(soft_list == NULL ) {
			continue;
		}
		do {
			soft_name = find_attr(soft_list,"name");
			if(!strcmp(soft_name,list)) {
				name =find_attr(current,"name");
				des = find_first_node(current,"description");
				/* Driver black list */
				i = 0;
				while( driver_black_list[i] != NULL ) {
					if(!strcmp(name,driver_black_list[i])){
						printf(" - driver %s(%s) is black-listed, skipping\n",des->data,name);
						break;
					}
					i++;
				}
				if( driver_black_list[i] != NULL ) {
					break;
				}

				/* Filter */
				drv = find_first_node(current,"driver");
				driver_status = find_attr(drv,"status");

				if(minyear > 0) {
					year = find_attr(drv,"year");
					if(year==NULL || year[0]==0){
						y = find_first_node(current,"year");
						year = y->data;
						if(year==NULL || year[0]==0){
							printf(" - driver %s(%s) : has no year, skipping\n",des->data,name);
							break;
						}
					}
					if( atoi(year) < minyear ) {
						printf(" - driver %s(%s) : is too old (%s), skipping\n",des->data,name,year);
						break;
					}
				}

				if(!strcmp(driver_status,"preliminary")) {
					if(!preliminarymode) {
						printf(" - driver %s(%s) is preliminary, skipping\n",des->data,name);
						break;
					}
					printf(" - driver %s(%s) is preliminary\n",des->data,name);
				}

				if(compatibility) {
					compat = find_attr(soft_list,"filter");
					if(compat) {
						if(strcmp(compat,compatibility)) {
							printf(" - driver %s(%s) is not compatible, skipping\n",des->data,name);
							break;
						}
					}
				}

				if( automode ) {
					i = 0;
					while(auto_black_list[i]!=NULL) {
						if(!strcmp(auto_black_list[i],name)) {
							printf("%s black listed for auto-mode\n",name);
							break;
						}
						i++;
					}

					if(auto_black_list[i]!=NULL){
						continue;
					}
				}

				driver[driver_count] = name;
				desc[driver_count] = des->data;
				driver_count++;
			}
		} while ( (soft_list=find_next_node(soft_list)) != NULL );

	} while ( (current=find_next_node(current)) != NULL );

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
	return driver[R];
}

/* return 1 if system is launched in NON auto mode, 0 otherwise */
int select_random_soft(int R)
{
	int count = 0;
	llist_t * current;
	llist_t * soft_list;
	llist_t * driver;
	llist_t * desc;
	llist_t * desc_soft;
	llist_t * part;
	llist_t * feature;
	llist_t * y;
	char * name;
	char * soft_name;
	char * ismechanical;
	char * isdevice;
	char * runnable;
	char * driver_status;
	char * year;
	char * list_desc;
	char * selected_driver;
	char * compatibility;
	char * feat;
	char buf[BUFFER_SIZE];
	int i;

	current = find_first_node(listxml,ENTRY_TYPE);
	do {
		if(count == R) {
			name = find_attr(current,"name");
			desc = find_first_node(current,"description");

			ismechanical = find_attr(current,"ismechanical");
			if(!strcmp(ismechanical,"yes")) {
				printf(" - %s(%s) is mechanical, skipping\n",desc->data,name);
				return 0;
			}
			isdevice = find_attr(current,"isdevice");
			if(!strcmp(isdevice,"yes")) {
				printf(" - %s(%s) is a device, skipping\n",desc->data,name);
				return 0;
			}
			runnable = find_attr(current,"runnable");
			if(!strcmp(runnable,"no")) {
				printf(" - %s(%s) is not runnable, skipping\n",desc->data,name);
				return 0;
			}

			driver = find_first_node(current,"driver");
			driver_status = find_attr(driver,"status");
			if(minyear > 0) {
				if(minyear > 0) {
					year = find_attr(driver,"year");
					if(year==NULL || year[0]==0){
						y = find_first_node(current,"year");
						year = y->data;
						if(year==NULL || year[0]==0){
							printf(" - driver %s(%s) : has no year, skipping\n",desc->data,name);
							return 0;
						}
					}
				}
				if( atoi(year) < minyear ) {
					printf(" - driver %s(%s) : is too old (%s), skipping\n",desc->data,name,year);
					return 0;
				}
			}
			if(!strcmp(driver_status,"preliminary")) {
				if(!preliminarymode) {
					printf(" - %s(%s) is preliminary, skipping\n",desc->data,name);
					return 0;
				}
				printf(" - %s(%s) is preliminary\n",desc->data,name);
			}

			/* Description black list */
			i = 0;
			while( desc_black_list[i] != NULL ) {
				if(strstr(desc->data,desc_black_list[i])){
					printf(" - driver %s(%s) is black-listed by its description, skipping\n",desc->data,name);
					break;
				}
				i++;
			}
			if( desc_black_list[i] != NULL ) {
			return 0;
			}

			/* check auto-mode black-list */
			if( automode ) {
				i = 0;
				while(auto_black_list[i]!=NULL) {
					if(!strcmp(auto_black_list[i],name)) {
						printf("%s black listed for auto-mode\n",name);
						return 0;
					}
					i++;
				}
			}

			printf("%s\n",desc->data);
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
	} while((current=find_next_node(current))!=NULL);

	if( softlist != NULL ) {
		current = find_first_node(softlist,SOFTWARELIST);
		do {
			soft_list = find_first_node(current,"software");
			do {
				if(count == R) {
					name = find_attr(current,"name");

					if( automode ) {
						i = 0;
						while(auto_black_softlist[i]!=NULL) {
							if(!strcmp(auto_black_softlist[i],name)) {
								printf("softlist %s black listed for auto-mode\n",name);
								break;
							}
							i++;
						}

						if(auto_black_softlist[i]!=NULL){
							return 0;
						}
					}

					list_desc = find_attr(current,"description");
					soft_name = find_attr(soft_list,"name");
					desc_soft = find_first_node(soft_list,"description");
#if 0
					supported = find_attr(soft_list,"supported");
					if(!strcmp(supported,"no")) {
						printf("\n - %s (%s:%s) software not supported, skipping\n",desc_soft->data,name,soft_name);
						return 0;
					}
#endif

					/* Try to get compatiblity feature */
					compatibility = NULL;
					part = find_first_node(soft_list,"part");
					feature = find_first_node(part,"feature");
					if( feature ) {
						do {
							feat = find_attr(feature,"name");
							if(!strcmp(feat,"compatibility")) {
								compatibility = find_attr(feature,"value");
								break;
							}
						} while ( (feature=find_next_node(feature))!=NULL);
					}

					printf("Software list: %s (%s)\n",list_desc,name);
					selected_driver = select_random_driver(name,compatibility);
					if( selected_driver == NULL ) {
						return 0;
					}
					printf("%s\n",desc_soft->data);
					sprintf(command_line,"%s %s",selected_driver,soft_name);
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
			} while((soft_list=find_next_node(soft_list))!=NULL);
		} while((current=find_next_node(current))!=NULL);
	}

	printf("Error!!");
	return 0;
}

void normal_mode()
{
	llist_t * current;
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

	current = find_first_node(listxml,ENTRY_TYPE);
	entry_count = 1;
	while((current=find_next_node(current))!=NULL) {
		entry_count++;
	}
	printf("%d entries\n",entry_count);

	if( softlist != NULL ) {
		softlist_count=0;
		software_count=0;
		current = find_first_node(softlist,SOFTWARELIST);
		do {
			softlist_count ++;
			count_forced_soft = 0;

			if( forced_list ) {
				list_name = find_attr(current,"name");
				if(!strcmp(forced_list,list_name)) {
					forced_list_start = software_count;
					count_forced_soft = 1;
				}
			}
			soft_list = find_first_node(current,"software");
			do {
				software_count++;
				if(count_forced_soft) {
					forced_list_count++;
				}
			} while((soft_list=find_next_node(soft_list))!=NULL);
		} while((current=find_next_node(current))!=NULL);
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

void read_chd_dir(char * dir)
{
        DIR * d;
        struct dirent * e;
	char buf[BUFFER_SIZE];

	d = opendir(dir);
	if(d==NULL) {
		printf("Error opening %s : %d\n",dir,errno);
		exit(1);
	}

	while( (e = readdir(d)) != 0 ) {
		if(!strcmp(e->d_name,".")) continue;
		if(!strcmp(e->d_name,"..")) continue;
		if(e->d_type == DT_DIR) {
			sprintf(buf,"%s/%s",dir,e->d_name);
			read_chd_dir(buf);
			continue;
		}
		if(e->d_type != DT_REG) continue;
		if(strlen(e->d_name)>4) {
			if(!strcmp(e->d_name+strlen(e->d_name)-4,".chd")) {
				chd_count++;
				chd_list_dir = realloc(chd_list_dir,sizeof(char*)*chd_count);
				chd_list_file = realloc(chd_list_file,sizeof(char*)*chd_count);
				chd_list_dir[chd_count-1]=strdup(dir);
				chd_list_file[chd_count-1]=strdup(e->d_name);
			}
		}
	}
	closedir(d);
}

void chd_mode()
{
	int R;
	char * tmp;
	char * tmp2;
	char context[128];
	char cmd[BUFFER_SIZE];
	char * driver;
	int i;

	if(forced_list) {
		sprintf(cmd,"%s/%s",roms_dir,forced_list);
		printf("Reading CHD files in %s\n",cmd);
		read_chd_dir(cmd);
	}
	else {
		printf("Reading CHD files in %s\n",roms_dir);
		read_chd_dir(roms_dir);
	}
	
	printf("%d CHD files found\n",chd_count);

	while (1) {
		R = rand();

		/* Set the context to the name of the directory */
		tmp = strstr(chd_list_dir[R%chd_count],"roms/");
		tmp+=(sizeof("roms/") - 1 );
		tmp2 = strstr(tmp,"/");
		if(tmp2 == NULL) {
			strcpy(context,tmp);
		}
		else {
			strncpy(context,tmp,tmp2-tmp);
			context[tmp2-tmp]=0;
		}

		/* chd mode filter (skipping black listed directories)*/
		i = 0;
		while(chd_black_list[i]!=NULL) {
			if(!strcmp(chd_black_list[i],context)) {
				printf("%s black listed for chd-mode\n",context);
				break;
			}
			i++;
		}
		if(chd_black_list[i]!=NULL) {
			continue;
		}

		printf("********************************\n");
		printf("%s\n\n",context);
		driver = select_random_driver(context,NULL);

		if(driver == NULL) {
			driver = context;
			printf("\nSetting driver name to directory name\n");
		}
	
		printf(	"\n%s\n%s\n",driver,chd_list_file[R%chd_count]);
		if(!automode) {
			sprintf(cmd,"%s %s %s -cdrom \"%s/%s\"\n",binary, option, driver, chd_list_dir[R%chd_count], chd_list_file[R%chd_count]);
			printf("Space to skip...\n");
			if( getchar() != 0x20 ) {
				printf("%s\n",cmd);
				if(system(cmd) == -1 ) {
					printf("Failed to run command %s\n",cmd);
				}
			}
		}
		else {
			sleep(1);
			sprintf(cmd,"%s %s %s %s -cdrom \"%s/%s\"",binary,option,auto_mode_option,driver, chd_list_dir[R%chd_count], chd_list_file[R%chd_count]);
			printf("%s\n",cmd);
			if(system(cmd) == -1 ) {
				printf("Failed to run command %s\n",cmd);
			}
		}
	}
}

void * launch_load_listxml(void * arg)
{
	parse_data_t data;
	char filename[1024];
	char cmd[1024];
	struct stat stat_info;
	char * type_info = PARAM_LISTXML;

	data.root_node = NULL;
	data.decoding_string = 0;
	data.xml_filter = NULL;
	data.current = NULL;

	sprintf(filename,"%s%s%s",tmp_dir,root_node,type_info);
	sprintf(cmd,"%s %s | tee %s ",binary,type_info,filename);

	if(stat(filename,&stat_info)==0 && !update) {
		sprintf(cmd,"/bin/cat %s",filename);
	}
	
	listxml = LoadXML(cmd,&data,filter);
	return NULL;
}

void * launch_load_getsoftlist(void * arg)
{
	parse_data_t data;
	char filename[1024];
	char cmd[1024];
	struct stat stat_info;
	char * type_info = PARAM_GETSOFTLIST;

	data.root_node = NULL;
	data.decoding_string = 0;
	data.xml_filter = NULL;
	data.current = NULL;

	sprintf(filename,"%s%s%s",tmp_dir,root_node,type_info);
	sprintf(cmd,"%s %s | tee %s ",binary,type_info,filename);

	if(stat(filename,&stat_info)==0 && !update ) {
		sprintf(cmd,"/bin/cat %s",filename);
	}
	
	softlist = LoadXML(cmd,&data,filter);
	return NULL;
}

void whitelist_mode()
{
	FILE * whitelist = NULL;
	char ** list = NULL;
	size_t len;
	int count = 0;
	char buf[1024];
	char * entry = NULL;
	int R;
	char driver[1024];
	int i;

	/* Load list */
	whitelist = fopen(white_list,"r");
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

void unset_terminal_mode(void)
{
	tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);
}

void init()
{

	char * tmp;
	char buf[BUFFER_SIZE];

	tmp_dir = get_tmp_dir();

	working_dir = getenv("MAME_WORKING_DIR");
	if(working_dir == NULL) {
		printf("Please set MAME_WORKING_DIR environnement variable");
		exit(-1);
	}

	tmp = getenv("MAME_BINARY");
	if(tmp == NULL) {
		printf("Please set MAME_BINARY environnement variable");
		exit(-1);
	}
	strncpy(buf,working_dir,BUFFER_SIZE);
	strncat(buf,"/",BUFFER_SIZE);
	strncat(buf,tmp,BUFFER_SIZE);
	binary = strdup(buf);

	roms_dir = getenv("MAME_ROMS_DIR");
	if(roms_dir == NULL) {
		printf("Please set MAME_ROMS_DIR environnement variable");
		exit(-1);
	}

	root_node = strdup(MAME_ROOT_NODE);

	tmp = getenv("HOME");
	if(tmp == NULL) {
		printf("Please set HOME environnement variable");
		exit(-1);
	}

	strncpy(buf,tmp,BUFFER_SIZE);
	strncat(buf,WHITE_LIST,BUFFER_SIZE);
	white_list=strdup(buf);

	printf("TMP_DIR:     %s\n",tmp_dir);
	printf("WORKING_DIR: %s\n",working_dir);
	printf("BINARY:      %s\n",binary);
	printf("ROMS_DIR:    %s\n",roms_dir);
	printf("WHITE_LIST:  %s\n",white_list);
}

int main(int argc, char**argv)
{
	int opt_ret;

	pthread_t thread_xml;
	pthread_t thread_softlist;

	void * ret;
	int duration = 60;

	srand ( time(NULL) );

	option[0] = 0;

	while((opt_ret = getopt_long(argc, argv, optstring, longopts, NULL))!=-1) {
		switch(opt_ret) {
			case 'a':
				automode = 1;
				break;
			case 'c':
				chdmode = 1;
				break;
			case 'p':
				preliminarymode = 1;
				break;
			case 'u':
				update = 1;
				break;
			case 'w':
				whitelistmode = 1;
				break;
			case 'l':
				forced_list = optarg;
				break;
			case 'y':
				minyear = atoi(optarg);
				break;
			case 'n':
				strcat(option,OPTION_NO_SOUND);
				break;
			case 'd':
				duration = atoi(optarg);
				break;
			default:
				printf("HELP:\n\n");
				printf("-a : automatic mode\n");
				printf("-c : only CHD\n");
				printf("-d <seconds> : auto mode run duration\n");
				printf("-l <list> : only use <list> software list\n");
				printf("-n : no sound\n");
				printf("-p : allow preliminary drivers\n");
				printf("-u : update cache from binaries\n");
				printf("-w : use white list\n");
				printf("-y <4 digit year> : only choose drivers later than <year>\n");
				exit(0);
		}
	}

	sprintf(auto_mode_option," %s %s %d ",AUTO_MODE_OPTION,DURATION_OPTION,duration);

	init();

	if(chdir(working_dir) == -1) {
		printf("Failed to change to directory %s\n",working_dir);
		exit(-1);
	}

	if( whitelistmode ) {
		whitelist_mode();
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

	if( chdmode) {
		chd_mode();
	}
	else {
		if(!automode) {
			whitelist = open(white_list,O_RDWR|O_CREAT,S_IRWXU|S_IRWXG|S_IROTH);
			if(whitelist == -1) {
				printf("Error openning whitelist\n");
				exit(1);
			}
			lseek(whitelist,0,SEEK_END);
		}
		normal_mode();
	}

	return 0;
}
