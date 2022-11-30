#!/bin/bash

max=$1
counter=1
s=$2

while [ $counter -le $max ]; do
	echo "Try $counter"
jsonString="{\
\"time\":\"`date`\",\
\"uuid\":\"`uuidgen`\"\
}"
	echo "[>>] $jsonString"
	printf "[<<] " && curl -X POST \
		--connect-timeout 2 \
		http://localhost:9000/uuid \
		-H 'Content-Type: application/json' \
		-d "$jsonString";
	printf "\n"
	counter=$(( $counter + 1 ))
#	echo ""
	if [[ "$s" -gt 0 ]]; then sleep $s; fi
done
