#!/bin/bash

#This script will parse the clockserver's config file to see how many regionservers to start then it will ( with a sleep time of $sleepTime seconds, as defined bellow ) start the clockserver, the world viewer and the region server(s).

#Usage: startup.sh serverType [debug] [run]

#possible serverType values: all, regionserver, clockserver, worldviewer
#passing "debug" will launch all the processes in gdb
#passing "debug run" will launch all the processes in gdb AND run them

#WARNING: this script MUST be run from your root antix directory

clear 

export directory=`dirname $0`
clockConfig="$directory/clockserver/config"
gdbCmd=""
sleepTime=1

if [ "$2" == "debug" ]; then
  gdbCmd="gdb -quiet --args "
  if [ "$3" == "run" ]; then
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

if [ "$1" == "clockserver" ] || [ "$1" == "all" ]; then
  echo -e "Starting the clock server with command: xterm -T \"Clockserver\" -e \"$gdbCmd$directory/clockserver/clockserver -c $directory/clockserver/config\" & \n"
  xterm -T "Clockserver" -e "$gdbCmd$directory/clockserver/clockserver -c $directory/clockserver/config" &
  sleep $sleepTime
fi

if [ "$1" == "worldviewer" ] || [ "$1" == "all" ]; then
  echo -e "Starting the world viewer with command: xterm -T \"World Viewer\" -e \"$gdbCmd$directory/worldviewer/worldviewer -c $directory/worldviewer/config\" & \n"
  xterm -T "World Viewer" -e "$gdbCmd$directory/worldviewer/worldviewer -c $directory/worldviewer/config" &
  sleep $sleep
fi

if [ "$1" == "regionserver" ] || [ "$1" == "all" ]; then
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
fi

exit 0

