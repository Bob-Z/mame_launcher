#!/bin/bash

gcc -Wall -g -O0 main.c parse_data.c linked_list.c misc.c -lexpat -pthread -o mame_launcher
