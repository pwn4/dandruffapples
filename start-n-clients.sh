#!/bin/bash

COUNT=$1
FIRST_TEAM=$2

IDX=0
PIDS=""
cd client
while [ $IDX -lt $COUNT ]
do
    ./client -t $[$IDX + $FIRST_TEAM] &
    PIDS="$PIDS $!"
    IDX=$[$IDX+1]
done

wait $PIDS
