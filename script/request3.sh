#!/bin/bash

max=$1
counter=1
s=$2

echo "Maximum request : $1"
echo "Delay between request : $s second(s)"
sleep 1

START_TIME=$(date)
SECONDS=0

while [ $counter -le $max ]; do
	echo "Try $counter/$max"
jsonString="{\
\"time\":\"`date`\",\
\"uuid\":\"`uuidgen`\"\
}"
	echo "[>>] $jsonString"
	printf "[<<] " && curl -X POST \
		--connect-timeout 2 \
		http://localhost:9000/ \
		-H 'Content-Type: application/json' \
		-d "$jsonString";
	printf "\n"
	if [[ "$counter" -eq $max ]]; then break; fi
	counter=$(( $counter + 1 ))
	sleep $s
done

END_TIME=$(date)
duration=$SECONDS
echo "Start Time : $START_TIME"
echo "End Time   : $END_TIME"
echo "Durations: $(($duration / 60)) minutes and $(($duration % 60)) seconds elapsed."