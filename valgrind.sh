#!/bin/bash

if [ "$#" -ne 1 ]; then
    printf "\n-- Binary file is mandatory! --\n\n"
    echo "Example :"
    printf "    $0 ./your/binary/file\n\n"
    exit 0
fi

fileType=$(file -b --mime-type $1 | sed 's|/.*||')
if [ "$fileType" != "application"  ]; then
    printf "\n-- '$1' is not a binary file! --\n\n"
    exit 0
fi

mkdir -p valgrind
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind/$1-valgrind-check.txt $1