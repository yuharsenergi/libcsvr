#!/bin/bash

valgrind --leak-check=full --track-origins=yes --log-file=$1-valgrind-check.txt $1