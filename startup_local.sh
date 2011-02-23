#!/bin/bash

#This script will parse the clockserver's config file to see how many regionservers to start then it will ( with a sleep time of $sleepTime seconds, as defined bellow ) start the clockserver, the world viewer and the region server(s).
#Usage: startup_local.sh [debug [run] ]
#not passing anything to the script will just normally run the regionserver(s), clockserver and worldserver
#passing  "debug" will launch all the processes in gdb
#passing "debug run" will launch all the processes in gdb AND run them
#WARNING: this script MUST be run from your root antix directory

clear 

export directory=`dirname $0`
clockConfig="$directory/clockserver/config"
gdbCmd=""
sleepTime=1

if [ "$1" == "debug" ]; then
  gdbCmd="gdb -quiet --args "
  if [ "$2" == "run" ]; then
    gdbCmd="gdb -quiet -ex run --args "
  fi
fi

while read inputline
do
  field="$(echo $inputline | cut -d' ' -f1)"
  if [ "$field" == "SERVERROWS"  ]; then
    serverRows="$(echo $inputline | cut -d' ' -f2)"
  elif [ "$field" == "SERVERCOLS" ]; then
    serverColumns="$(echo $inputline | cut -d' ' -f2)"
  fi 
done < $clockConfig

export regionServers=`expr $serverRows \* $serverColumns`


export gdbCmd

echo -e "Starting the clock server with command: xterm -T \"Clockserver\" -e \"$gdbCmd$directory/clockserver/clockserver -c $directory/clockserver/config\" & \n"
xterm -T "Clockserver" -e "$gdbCmd$directory/clockserver/clockserver -c $directory/clockserver/config" &
sleep $sleepTime
echo -e "Starting the world viewer with command: xterm -T \"World Viewer\" -e \"$gdbCmd$directory/worldviewer/worldviewer -c $directory/worldviewer/config\" & \n"
xterm -T "World Viewer" -e "$gdbCmd$directory/worldviewer/worldviewer -c $directory/worldviewer/config" &
sleep $sleepTime

config="$directory/regionserver/config"
for((i=1; i<=$regionServers; i++ ));do
  if [ $i -eq 1 ]; then
    echo -e "Starting the region server $i with command: xterm -T \"Region Server: #$i\" -e \"$gdbCmd$directory/regionserver/regionserver -c $config\" & \n"
    xterm -T "Region Server: #$i" -e "$gdbCmd$directory/regionserver/regionserver -c $config" &
    sleep $sleepTime
  else
    echo -e "Starting the region server $i with command: xterm -T \"Region Server: #$i\" -e \"$gdbCmd$directory/regionserver/regionserver -c $config$i\" & \n"
    xterm -T "Region Server: #$i" -e "$gdbCmd$directory/regionserver/regionserver -c $config$i" &
    sleep $sleepTime
  fi
done

exit 0

