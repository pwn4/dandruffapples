#!/bin/bash

COUNT=$1
CLOCK=$2
echo "Launching $COUNT regions towards clock $CLOCK"

IDX=1
PIDS=""
cd regionserver
while [ $IDX -le $COUNT ]
do
    CONFIDX=$IDX
    if [ $IDX -eq 1 ]; then
        CONFIDX=""
    fi
    ./regionserver -l "$CLOCK" -c "config$CONFIDX" &
    PIDS="$PIDS $!"
    IDX=$[$IDX+1]
done

wait $PIDS
