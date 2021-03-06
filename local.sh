#!/bin/bash

#This script will parse the clockserver's config file to start the Antix system based on the arguments passed to it

#Usage: local.sh [argument1] [argument2] ...

#ARGUMENTS:
#-shortcut: will start with the following arguments: -all -sleepTime 0.2 -gnomeTerminal -debug -run
#-all: run the clockserver, worldviewer, controller, regionserver(s), client(s)
#-clockserver: run the clockserver
#-controller: run the controller
#-worldviewer: run the worldviewer
#-regionserver: run the regionserver(s)
#-client: run the client(s)
#-clientviewer: works ONLY if -client has been passed somewhere prior to it. Will start the client(s) with the -viewer option
#-debug: launch all the server in gdb
#-run: works ONLY if -debug has been passed somewhere prior to it. Will tell gdb to auto-run all the servers
#-gnomeTerminal: if passed then all the servers will be open using the "gnome-terminal" ( default: xterm )
#-sleepTime: time in seconds to wait before launching a new server ( default: 0 )
#-callgrind: launch the servers in vallgrind with the callgrind tool
#-memcheck: launch the servers in vallgrind with the memcheck tool

#EXAMPLES:
#./local.sh -clockserver -worldivewer -regionserver
#will launch only the clockserver, worldviewer and the regionserver(s) in xterm

#./local.sh -clockserver -gnomeTerminal
#will launch only the clockserver in gnome-terminal

#./local.sh -client -debug
#will launch only the client(s) in xterm, in gdb, but will not auto-run

#./local.sh -shortcut
#will launch the clockserver, worldviewer, controller, regionserver(s), client(s) in the gnometerminal, in gdb with the auto-run feature enabled and at an interval of 0.2 seconds


#WARNING: this script MUST be run from your root antix directory

clear 

directory=`dirname $0`
clockConfig="$directory/clockserver/config"
#default sleep time is one second
sleepTime=2
#default terminal is xterm
terminal="xterm -T"

#figure out what flags need to be set from the arguments passed
while [ "$1" != "" ]; do
echo "Found \"${1,,}\" argument"
  if [ "${1,,}" == "-shortcut" ]; then
    clockserver=true
    regionserver=true
    controller=true
    client=true
    debug=true
    run=true
    sleepTime=0.5
  elif [ "${1,,}" == "-all" ]; then
    clockserver=true
    regionserver=true
    controller=true
    client=true
  elif [ "${1,,}" == "-clockserver" ]; then
    clockserver=true
  elif [ "${1,,}" == "-worldviewer" ]; then
    worldviewer=true
  elif [ "${1,,}" == "-regionserver" ]; then
    regionserver=true
  elif [ "${1,,}" == "-controller" ]; then
    controller=true
  elif [ "${1,,}" == "-client" ]; then
    client=true
  elif [ "${1,,}" == "-debug" ]; then
    debug=true
  elif [ "${1,,}" == "-run" ]; then
    if [ $debug ]; then
      run=true
    else
      echo "Cannot specify the \"-run\" argument without a preceding \"-debug\" argument"
    fi
  elif [ "${1,,}" == "-clientviewer" ]; then
    if [ $client ]; then
      clientViewer="-viewer"
    else
      echo "Cannot specify the \"-clientViewer\" argument without a preceding \"-client\" argument"
    fi
  elif [ "${1,,}" == "-gnometerminal" ]; then
    gnomeTerminal=true
  elif [ "${1,,}" == "-callgrind" ]; then
    callgrind=true
  elif [ "${1,,}" == "-memcheck" ]; then
    memcheck=true
  elif [ "${1,,}" == "-sleeptime" ]; then
    shift
    sleepTime=$1
  else
    echo "${1,,} is not a valid argument. Please read the documentation in this script"
  fi

  shift
done

if [ $debug ]; then
    cmd="gdb -quiet --args "
    
  if [ $run ]; then
    cmd="gdb -quiet -ex run --args "    
  fi
fi

if [ $callgrind ]; then
  cmd="valgrind --tool=callgrind --callgrind-out-file=/tmp/antix_callgrind_logs/callgrind.out."
  mkdir /tmp/antix_callgrind_logs/
fi

if [ $memcheck ]; then
  cmd="valgrind --tool=memcheck --leak-check=full --log-file=/tmp/antix_memcheck_logs/"
  mkdir /tmp/antix_memcheck_logs/
fi

if [ $gnomeTerminal ]; then
  terminal="gnome-terminal -t"
fi

while read inputline
do
  field="$(echo $inputline | cut -d' ' -f1)"
  if [ "$field" == "SERVERROWS"  ]; then
    serverRows="$(echo $inputline | cut -d' ' -f2)"
  elif [ "$field" == "SERVERCOLS" ]; then
    serverColumns="$(echo $inputline | cut -d' ' -f2)"
  elif [ "$field" == "TEAMS" ]; then
    teams="$(echo $inputline | cut -d' ' -f2)"
  fi 
done < $clockConfig

if [ $clockserver ]; then
  killall clockserver
  killall controller
  killall worldviewer
  killall regionserver
  killall client

  if [ $memcheck ] || [ $callgrind ]; then 
    cmdArgs="clockserver.%p "
  fi

  echo -e "Starting the clock server with command:\n$terminal \"Clockserver\" -e \"$cmd$cmdArgs$directory/clockserver/clockserver -c $directory/clockserver/config\" & \n"
  $terminal "Clockserver" -e "$cmd$cmdArgs$directory/clockserver/clockserver -c $directory/clockserver/config" &
  sleep $sleepTime
fi

if [ $controller ]; then
  if [ $memcheck ] || [ $callgrind ]; then 
    cmdArgs="controller.%p "
  fi

  echo -e "Starting the controller with command:\n$terminal \"Controller\" -e \"$cmd$cmdArgs$directory/controller/controller -c $directory/controller/config\" & \n"
  $terminal "Controller" -e "$cmd$cmdArgs$directory/controller/controller -c $directory/controller/config" &
  sleep $sleepTime
fi

if [ $worldviewer ]; then
  if [ $memcheck ] || [ $callgrind ]; then 
    cmdArgs="worldviewer.%p "
  fi

  echo -e "Starting the world viewer with command:\n$terminal \"World Viewer\" -e \"$cmd$cmdArgs$directory/worldviewer/worldviewer -c $directory/worldviewer/config\" & \n"
  $terminal "World Viewer" -e "$cmd$cmdArgs$directory/worldviewer/worldviewer -c $directory/worldviewer/config" &
  sleep $sleepTime
fi

if [ $regionserver ]; then
  config="$directory/regionserver/config"
  regionServers=`expr $serverRows \* $serverColumns`

  for((i=1; i<=$regionServers; i++ ));do

  if [ $memcheck ] || [ $callgrind ]; then 
    cmdArgs="regionserver$i.%p "
  fi

    if [ $i -eq 1 ]; then
      echo -e "Starting the region server $i with command:\n$terminal \"Region Server: #$i\" -e \"$cmd$cmdArgs$directory/regionserver/regionserver -c $config\" & \n"
      $terminal "Region Server #$i" -e "$cmd$cmdArgs$directory/regionserver/regionserver -c $config" &
      sleep $sleepTime
    else
      echo -e "Starting the region server $i with command:\n$terminal \"Region Server: #$i\" -e \"$cmd$cmdArgs$directory/regionserver/regionserver -c $config$i\" & \n"
      $terminal "Region Server #$i" -e "$cmd$cmdArgs$directory/regionserver/regionserver -c $config$i" &
      sleep $sleepTime
    fi
  done
fi

if [ $client ]; then
  for((i=0; i<$teams; i++ ));do

  if [ $memcheck ] || [ $callgrind ]; then 
    cmdArgs="client$i.%p "
  fi

    echo -e "Starting the client with command:\n$terminal \"Client controlling team  #$i\" -e \"$cmd$cmdArgs$directory/client/client -c $directory/client/config -t $i $clientViewer\" & \n"
    $terminal "Client controlling team #$i" -e "$cmd$cmdArgs$directory/client/client -c $directory/client/config -t $i $clientViewer" &
    sleep $sleepTime
  done
fi

exit 0

