#!/bin/bash

max=$1
counter=1
s=$2
if [[ s -eq 0 ]]
then s=0.5
fi
while [ $counter -le $max ]; do
#while true; do
	echo "Try $counter"
	curl -X POST --connect-timeout 2 http://localhost:9000/uuid -H 'Content-Type: application/json' -d '{"message":"hello"}'
	counter=$(( $counter + 1 ))
	echo ""
	sleep .$s
done
