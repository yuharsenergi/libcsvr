#!/bin/bash

valgrind --leak-check=full --track-origins=yes --log-file=leak.txt $1