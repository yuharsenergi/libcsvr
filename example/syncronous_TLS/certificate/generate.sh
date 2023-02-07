#!/bin/bash

if [ "$#" -lt 1 ]; then
    printf "\n-- output filename is mandatory! --\n\n"
    echo "Example :"
    printf "    $0 server.pem server.key\n\n"
    exit 0
fi

CERTIFICATE_FILE=$1
PRIVATE_KEY_FILE=$1

if [ "$#" -eq 2 ]; then
    PRIVATE_KEY_FILE=$2
fi

openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout certificate/$CERTIFICATE_FILE -out certificate/$PRIVATE_KEY_FILE