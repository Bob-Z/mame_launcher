/*
   mame-launcher runs MAME emulator with random machines.
   Copyright (C) 2013-2016 carabobz@gmail.com

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

#define CONFIG_DIR "/.config/mame_launcher"
#define WHITE_LIST CONFIG_DIR "/whitelist"
#define VERSION_FILENAME CONFIG_DIR "/version"
#define CACHE_LISTXML CONFIG_DIR "/listxml"
#define CACHE_GETSOFTLIST CONFIG_DIR "/getsoftlist"
#define OPTION_NO_SOUND " -nosound "
#define AUTO_MODE_OPTION "-nowindow -ui_active "
#define DURATION_OPTION "-str"

#define BUFFER_SIZE (10000)
#define MAX_DRIVER (100000)

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
char * whitelist_filename=NULL;
char * version_filename=NULL;
char * cache_listxml=NULL;
char * cache_getsoftlist=NULL;

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
char * auto_black_list[] = { "cdimono1", "cpc464", "cpc664", "cpc6128", "al520ex", "kccomp", "cpc6128f", "cpc6128s", "bbcb_de", "z1013", NULL } ;
/* Names of softlist to skip when running in auto mode */
char * auto_black_softlist[] = { "tvc_flop", "ti99_cart", "bbca_cass", "bbcb_cass", "msx1_cass", "lviv", "kc_cass", "spc1000_cass", "sol20_cass", "mtx_cass", "dai_cass", "ep64_flop", "pet_cass", "orion_cass", "mz700_cass", "special_cass", "cbm2_flop", "cgenie_cass", "jupace_cass", "bbcm_flop", "fm7_cass", "mc10", "x07_cass", NULL } ;
/* Names of drivers to skip when selecting a driver */
char * driver_black_list[] = { "kccomp", "al520ex", "cpc6128s", "cpc6128f", NULL };
/* Description string to black list (namely for fruit machine) */
char * desc_black_list[] = { "(MPU4)", "(Scorpion 4)", "Player's Edge Plus", NULL };

int automode = FALSE;
int chdmode = FALSE;
int preliminarymode = FALSE;
int minyear = FALSE;
int whitelistmode = FALSE;
int coinonly = FALSE;
int gambling = FALSE;

struct termios orig_term_attr;
struct termios new_term_attr;

const char optstring[] = "?acpwl:y:nd:og";
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
	{NULL,0,NULL,0}};


static char *filter[]= { "dipswitch", "dipvalue", "chip", "display", "sound", "configuration", "confsetting", "adjuster", "device", "instance", "extension", "slot", "slotoption", "ramoption", "publisher", "info", "sharedfeat", "manufacturer", "biosset", "device_ref", "sample", NULL };

static int is_machine_ok(llist_t * machine)
{
	llist_t * desc;
	llist_t * drv;
	llist_t * input;
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
	int i;

	name =find_attr(machine,"name");
	desc = find_first_node(machine,"description");

	// Driver black list
	i = 0;
	while( driver_black_list[i] != NULL ) {
		if(!strcmp(name,driver_black_list[i])){
			printf(" - driver %s(%s) is black-listed, skipping\n",desc->data,name);
			return FALSE;
		}
		i++;
	}

	// coin only
	input = find_first_node(machine,"input");
	if(coinonly) {
		coins = find_attr(input,"coins");

		if( coins==NULL || coins[0]==0 ) {
			printf(" - driver %s(%s) : is not coin operated, skipping\n",desc->data,name);
			return FALSE;
		}
	}

	// gambling
	if(!gambling) {
		control = find_first_node(input,"control");
		while( control ) {
			type = find_attr(control,"type");
			if(!strcmp(type,"gambling")){
				printf(" - driver %s(%s) is gambling, skipping\n",desc->data,name);
				return FALSE;
			}
			control = find_next_node(control);
		}
	}

	//minimal year
	if(minyear > 0) {
		y = find_first_node(machine,"year");
		year = y->data;
		if(year==NULL || year[0]==0){
			printf(" - driver %s(%s) : has no year, skipping\n",desc->data,name);
			return FALSE;
		}

		if( atoi(year) < minyear ) {
			printf(" - driver %s(%s) : is too old (%s), skipping\n",desc->data,name,year);
			return FALSE;
		}
		printf(" - %s(%s) year is %s\n",desc->data,name,year);
	}

	// driver status
	drv = find_first_node(machine,"driver");
	driver_status = find_attr(drv,"status");
	if(!strcmp(driver_status,"preliminary")) {
		if(!preliminarymode) {
			printf(" - driver %s(%s) is preliminary, skipping\n",desc->data,name);
			return FALSE;
		}
		printf(" - driver %s(%s) is preliminary\n",desc->data,name);
	}

	// auto mode
	if( automode ) {
		i = 0;
		while(auto_black_list[i]!=NULL) {
			if(!strcmp(auto_black_list[i],name)) {
				printf("%s black listed for auto-mode\n",name);
				return FALSE;
			}
			i++;
		}
	}

	// mechanical
	ismechanical = find_attr(machine,"ismechanical");
	if(!strcmp(ismechanical,"yes")) {
		printf(" - %s(%s) is mechanical, skipping\n",desc->data,name);
		return FALSE;
	}

	// device
	isdevice = find_attr(machine,"isdevice");
	if(!strcmp(isdevice,"yes")) {
		printf(" - %s(%s) is a device, skipping\n",desc->data,name);
		return FALSE;
	}

	// runnable
	runnable = find_attr(machine,"runnable");
	if(!strcmp(runnable,"no")) {
		printf(" - %s(%s) is not runnable, skipping\n",desc->data,name);
		return FALSE;
	}

	// Description black list
	i = 0;
	while( desc_black_list[i] != NULL ) {
		if(strstr(desc->data,desc_black_list[i])){
			printf(" - driver %s(%s) is black-listed by its description, skipping\n",desc->data,name);
			return FALSE;
		}
		i++;
	}

	return TRUE;
}

static char * select_random_driver(char * list, char * compatibility)
{
	llist_t * machine;
	llist_t * soft_list;
	llist_t * des;
	char * name;
	char * soft_name;
	char * compat;
	char * driver[MAX_DRIVER];
	char * desc[MAX_DRIVER];
	int driver_count = 0;
	int R;
	int i;

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

				driver[driver_count] = name;
				desc[driver_count] = des->data;
				driver_count++;
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
	return driver[R];
}

/* return 1 if system is launched in NON auto mode, 0 otherwise */
static int select_random_soft(int R)
{
	int count = 0;
	llist_t * machine;
	llist_t * soft_list;
	llist_t * desc;
	llist_t * desc_soft;
	llist_t * part;
	llist_t * feature;
	char * name;
	char * soft_name;
	char * list_desc;
	char * selected_driver;
	char * compatibility;
	char * feat;
	char buf[BUFFER_SIZE];
	int i;

	machine = find_first_node(listxml,ENTRY_TYPE);
	do {
		if(count == R) {
			if( ! is_machine_ok(machine) ) {
				return 0;
			}

			name = find_attr(machine,"name");
			desc = find_first_node(machine,"description");

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
	} while((machine=find_next_node(machine))!=NULL);

	if( softlist != NULL ) {
		machine = find_first_node(softlist,SOFTWARELIST);
		do {
			soft_list = find_first_node(machine,"software");
			do {
				if(count == R) {
					name = find_attr(machine,"name");

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

					list_desc = find_attr(machine,"description");
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
		} while((machine=find_next_node(machine))!=NULL);
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

	if( softlist != NULL ) {
		softlist_count=0;
		software_count=0;
		machine = find_first_node(softlist,SOFTWARELIST);
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

static void read_chd_dir(char * dir)
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

static void chd_mode()
{
	int R;
	char * tmp;
	char * tmp2;
	char context[128];
	char cmd[BUFFER_SIZE];
	char cdrom_opt[BUFFER_SIZE];
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
			sprintf(cdrom_opt," ");
		}
		else {
			sprintf(cdrom_opt," -cdrom \"%s/%s\"",chd_list_dir[R%chd_count], chd_list_file[R%chd_count]);
		}
	
		printf(	"\n%s\n%s\n",driver,chd_list_file[R%chd_count]);
		if(!automode) {
			sprintf(cmd,"%s %s %s %s\n",binary, option, driver, cdrom_opt);
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
			sprintf(cmd,"%s %s %s %s %s",binary,option,auto_mode_option,driver,cdrom_opt);
			printf("%s\n",cmd);
			if(system(cmd) == -1 ) {
				printf("Failed to run command %s\n",cmd);
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
	softlist = LoadXML(cache_getsoftlist,filter);
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

static void init()
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

	tmp = getenv("HOME");
	if(tmp == NULL) {
		printf("Please set HOME environnement variable");
		exit(-1);
	}

	strncpy(buf,tmp,BUFFER_SIZE);
	strncat(buf,WHITE_LIST,BUFFER_SIZE);
	whitelist_filename=strdup(buf);

	strncpy(buf,tmp,BUFFER_SIZE);
	strncat(buf,VERSION_FILENAME,BUFFER_SIZE);
	version_filename=strdup(buf);

	strncpy(buf,tmp,BUFFER_SIZE);
	strncat(buf,CACHE_LISTXML,BUFFER_SIZE);
	cache_listxml=strdup(buf);

	strncpy(buf,tmp,BUFFER_SIZE);
	strncat(buf,CACHE_GETSOFTLIST,BUFFER_SIZE);
	cache_getsoftlist=strdup(buf);

	printf("TMP_DIR:           %s\n",tmp_dir);
	printf("WORKING_DIR:       %s\n",working_dir);
	printf("BINARY:            %s\n",binary);
	printf("ROMS_DIR:          %s\n",roms_dir);
	printf("WHITE_LIST:        %s\n",whitelist_filename);
	printf("VERSION:           %s\n",version_filename);
	printf("CACHE_LISTXML:     %s\n",cache_listxml);
	printf("CACHE_GETSOFTLIST: %s\n",cache_getsoftlist);
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
	printf("Current MAME version is: %s\n",buf_new);

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

int main(int argc, char**argv)
{
	int opt_ret;
	char buffer[BUFFER_SIZE];

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
				printf("-g : allow gambling games (default is no gambling game)\n");
				printf("-l <list> : only use <list> software list\n");
				printf("-n : no sound\n");
				printf("-o : coin operated nachine only\n");
				printf("-p : allow preliminary drivers\n");
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

	if( chdmode) {
		chd_mode();
	}
	else {
		if(!automode) {
			mkdir(CONFIG_DIR,0777);
			whitelist = open(whitelist_filename,O_RDWR|O_CREAT,S_IRWXU|S_IRWXG|S_IROTH);
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
