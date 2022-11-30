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

fileName=$1
BIN=$1

if [[ "$1" == *"/"* ]];
then
    fileName=$($1 | rev | cut -d"/" -f1 | rev)
fi

logFile="valgrind/$fileName-valgrind-check.txt"

mkdir -p $(pwd)/valgrind
echo "File location : $1"
echo "Filename : $fileName"
echo "Logfile  : $logFile"
echo "valgrind --leak-check=full --show-leak-kinds=all --verbose --log-file=$logFile $BIN"
valgrind --leak-check=full --show-leak-kinds=all --verbose --log-file=$logFile $BIN
