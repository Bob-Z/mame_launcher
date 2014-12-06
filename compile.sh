#!/bin/bash

#gcc -Wall -O2 main.c parse_data.c linked_list.c -lexpat -pthread
gcc -Wall -g -O0 main.c parse_data.c linked_list.c -lexpat -pthread
#gcc -g -O0 main.c parse_data.c linked_list.c `xml2-config --cflags` `xml2-config --libs`
