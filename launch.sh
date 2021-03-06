#!/bin/bash

#DEBUG=1

# Port remote hosts' sshds are listening on
SSHPORT=24
# Path of project dir relative to $HOME
PROJDIR="dandruffapples"
# Path to write logs
LOGDIR="logs"
# Path to write other files
OUTPATH="tmp"
# Path to write generated clock config to
CLOCKCONF="$OUTPATH/antix-clock-conf"
# Path to read CSIL hosts from
HOSTFILE="./hosts"
# Number of regions to launch on a single machine
REGIONS_PER_HOST=2
#Number of clients to launch per host
#CLIENTSPERHOST=10

#link the shared object library here in case
LD_LIBRARY_PATH='$PROJDIR/sharedlibs'
export LD_LIBRARY_PATH

if [ ! -e "$HOSTFILE" ]
then
    echo "\$HOSTFILE is set to \"$HOSTFILE\", which does not exist!"
    exit 1
fi

if [ $# -lt 2 ]
then
    echo "Usage: $0 <columns> <rows> [teams] [team size] [controllers]"
    echo "If no controller count is specified, no controllers or clients will be launched."
    exit 1
fi

SSHPROCS=""
CLOCKID=""
function cleanup {
    echo "Cleaning up."
    if [ $CLOCKID ]
    then
        kill $CLOCKID 2> /dev/null
    fi
    for PID in $SSHPROCS
    do
        kill $PID 2> /dev/null
    done
}

function wrap {
    if [ $DEBUG ]
    then
        xterm -e "$@"
    else
        $@
    fi
}

COLS=$1
ROWS=$2
TEAMS=$3
TEAMSIZE=$4
CONTROLLERS=$5

if [ -z $TEAMS ]
then
    TEAMS=4
fi
if [ -z $TEAMSIZE ]
then
    TEAMSIZE=1000
fi

cat > $CLOCKCONF <<EOF
SERVERROWS ${ROWS}
SERVERCOLS ${COLS}
TEAMS ${TEAMS}
ROBOTS_PER_TEAM ${TEAMSIZE}
EOF

DEBUGGER=""
if [ $DEBUG ]
then
    DEBUGGER="gdb -quiet -ex run --args"
fi


if [ -e "$LOGDIR" ]
then
    # Clobber logs
    rm -rf "$LOGDIR"
fi
mkdir -p "$LOGDIR"

echo "Starting clock server locally."
CLOCKSERVER=`host \`hostname\`|cut -d ' ' -f 4`
mkdir -p "$OUTPATH"
set -m
wrap $DEBUGGER clockserver/clockserver -c "$CLOCKCONF" &
CLOCKID=$!
trap "echo -e '\nCaught signal; shutting down.' && cleanup; exit 1" HUP INT TERM

sleep 0.1
if [ ! -e /proc/$CLOCKID ]
then
    echo "Clock server failed to start!"
    exit 1
fi

SSHCOMMAND="ssh -o StrictHostKeyChecking=no -o PasswordAuthentication=no -o ConnectTimeout=1 -p $SSHPORT"

REGIONS_LEFT=$[$COLS * $ROWS]
CONTROLLERS_LEFT=0;
if [ $CONTROLLERS ]
then
    CONTROLLERS_LEFT=$CONTROLLERS
fi
CONTROLHOSTS=""
#CLIENTS_LEFT=$TEAMS
echo "Launching $CONTROLLERS_LEFT controllers and $REGIONS_LEFT regions"
for HOST in `grep -hv \`hostname\` "$HOSTFILE"`
do
    if [ ! -e /proc/$CLOCKID ]
    then
        echo "Clock server died!  Halting."
        cleanup
        exit 1
    fi
    echo "- Trying $HOST"
    #check for host being up
    INUSE=""
    if ! $SSHCOMMAND $HOST "true"
    then
	echo "- Skipping unresponsive host $HOST"
        continue
    fi

    if [ $CONTROLLERS_LEFT -gt 0 ]
    then
        sleep 0.1
        CONTROLLERS_LEFT=$[$CONTROLLERS_LEFT - 1]
        echo "Launching controller on $HOST ($CONTROLLERS_LEFT remaining)"
        wrap $SSHCOMMAND $HOST "bash -c \"cd '$PROJDIR/controller' && LD_LIBRARY_PATH='$PROJDIR/sharedlibs' $DEBUGGER ./controller -l $CLOCKSERVER\"" > "$LOGDIR/controller.out.$HOST.log" 2> "$LOGDIR/controller.err.$HOST.log" &
        SSHPROCS="$SSHPROCS $!"
        CONTROLHOSTS="$CONTROLHOSTS $HOST"
    elif [ $REGIONS_LEFT -gt 0 ]
    then
        sleep 0.1
        REGIONCOUNT=$REGIONS_PER_HOST
        if [ $REGIONS_LEFT -lt $REGIONS_PER_HOST ]
        then
            REGIONCOUNT=$REGIONS_LEFT
        fi
        REGIONS_LEFT=$[$REGIONS_LEFT - $REGIONCOUNT]
        echo "Launching $REGIONCOUNT regions on $HOST ($REGIONS_LEFT remaining)"
        wrap $SSHCOMMAND $HOST "bash -c \"cd '$PROJDIR' && ./start-n-regions.sh $REGIONCOUNT $CLOCKSERVER\"" > "$LOGDIR/regiongroup.out.$HOST.log" 2> "$LOGDIR/regiongroup.err.$HOST.log" &
    fi

    if [ $CONTROLLERS_LEFT -eq 0 ] && [ $REGIONS_LEFT -eq 0 ]
    then
        echo "All regions and controllers launched!"
        break
    fi
done

if [ $REGIONS_LEFT -ne 0 -o $CONTROLLERS_LEFT -ne 0 ]
then
    echo "Ran out of hosts before we got all servers running!  Shutting down."
    cleanup
    exit 1
fi

sleep 10
if [ $CONTROLLERS ]
then
   echo "Launching $TEAMS clients across $CONTROLLERS machines"
   # One shell per machine
   CLIENTS_LEFT=$TEAMS
   QUOTIENT=$[$TEAMS / $CONTROLLERS]
   REMAINDER=$[$TEAMS % $CONTROLLERS]
   HOSTIDX=1
   while [ $CLIENTS_LEFT -gt 0 ]
   do
       if [ ! -e /proc/$CLOCKID ]
       then
           echo "Clock server died!  Halting."
           cleanup
           exit 1
       fi
       CLIENTS_LEFT=$[$CLIENTS_LEFT - $QUOTIENT]
       EXTRA=0
       if [ $REMAINDER -gt 0 ]
       then
           EXTRA=1
           REMAINDER=$[$REMAINDER - 1]
           CLIENTS_LEFT=$[$CLIENTS_LEFT - 1]
       fi
       
       HOST=`echo $CONTROLHOSTS |cut -d ' ' -f $HOSTIDX`
       echo "Launching $[$QUOTIENT + $EXTRA] clients on controller $HOST ($CLIENTS_LEFT remaining)"
       wrap $SSHCOMMAND $HOST "bash -c \"cd '$PROJDIR' && ./start-n-clients.sh $[$QUOTIENT + $EXTRA] $CLIENTS_LEFT\"" > "$LOGDIR/clientgroup.out.$HOST.log" 2> "$LOGDIR/clientgroup.err.$HOST.log" &
       SSHPROCS="$SSHPROCS $!"
       HOSTIDX=$[$HOSTIDX+1]
       sleep 0.1
   done

   echo "All clients launched!"
fi

echo "All done!  Here's the clock server."
fg 1

cleanup
