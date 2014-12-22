#!/bin/bash

gcc -Wall -g -O0 main.c parse_data.c linked_list.c -lexpat -pthread -o ume_launcher
