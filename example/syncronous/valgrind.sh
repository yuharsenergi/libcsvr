#!/bin/bash

echo "Total argument $#"
if [ "$#" -lt 1 ]; then
    printf "\n-- Binary file is mandatory! --\n\n"
    echo "Example :"
    printf "    $0 ./your/binary/file\n\n"
    exit 0
fi

if [ "$#" -lt 2 ]; then
    printf "\n-- Port definition is mandatory! --\n\n"
    echo "Example :"
    printf "    $0 $1 9000\n\n"
    exit 0
fi

fileType=$(file -b --mime-type $1 | sed 's|/.*||')
if [ "$fileType" != "application"  ]; then
    printf "\n-- '$1' is not a binary file! --\n\n"
    exit 0
fi

BIN=$1
PORT=$2

logFile="valgrind/syncronous-valgrind-check.txt"

echo "File location : $(pwd)/$1"
mkdir -p $(pwd)/valgrind
echo "Filename : $fileName"
echo "Logfile  : $logFile"
echo "valgrind --leak-check=full --show-leak-kinds=all --verbose --log-file=$logFile $BIN $PORT"
valgrind --leak-check=full --show-leak-kinds=all --verbose --log-file=$logFile $BIN $PORT
