#!/bin/bash

#DEBUG=1

# Port remote hosts' sshds are listening on
SSHPORT=24
# Path of project dir relative to $HOME
PROJDIR="dandruffapples"
# Path to write other files
OUTPATH="tmp"
# Path to write generated clock config to
CLOCKCONF="$OUTPATH/antix-clock-conf"
# Path to read CSIL hosts from
HOSTFILE="./hosts"
# Number of regions to launch on a single machine
REGIONS_PER_HOST=2
#Number of clients to launch per host
CLIENTSPERHOST=10

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

SSHCOMMAND="ssh -o StrictHostKeyChecking=no -o PasswordAuthentication=no -p $SSHPORT"

REGIONS_LEFT=$[$COLS * $ROWS]
CONTROLLERS_LEFT=0;
if [ $CONTROLLERS ]
then
    CONTROLLERS_LEFT=$CONTROLLERS
fi
CONTROLHOSTS=""
CLIENTS_LEFT=$TEAMS
echo -n "Launching $CONTROLLERS_LEFT controllers and $REGIONS_LEFT regions"
for HOST in `grep -hv \`hostname\` "$HOSTFILE"`
do
    if (! host "$HOST" >/dev/null) || (! nc -z "$HOST" $SSHPORT)
    then
        continue
    fi
    
    if [ $CONTROLLERS_LEFT -gt 0 ]
    then
        wrap $SSHCOMMAND $HOST "bash -c \"cd '$PROJDIR/controller' && LD_LIBRARY_PATH='$PROJDIR/sharedlibs' $DEBUGGER ./controller -l $CLOCKSERVER\"" > /dev/null &
        SSHPROCS="$SSHPROCS $!"
        echo -n .
        CONTROLLERS_LEFT=$[$CONTROLLERS_LEFT - 1]
        CONTROLHOSTS="$CONTROLHOSTS $HOST"
    elif [ $REGIONS_LEFT -gt 0 ]
    then
        DECREMENTED=$[$REGIONS_LEFT - $REGIONS_PER_HOST]
        CONFIDX=1
        while [ $REGIONS_LEFT -gt $DECREMENTED -a $REGIONS_LEFT -gt 0 ]
        do
            if [ $CONFIDX -eq 1 ]
            then
		sleep 0.5
                wrap $SSHCOMMAND $HOST "bash -c \"cd '$PROJDIR/regionserver' && LD_LIBRARY_PATH='$PROJDIR/sharedlibs' $DEBUGGER ./regionserver -l $CLOCKSERVER -c config\"" > /dev/null &
                SSHPROCS="$SSHPROCS $!"
            else
		sleep 0.5
                wrap $SSHCOMMAND $HOST "bash -c \"cd '$PROJDIR/regionserver' && LD_LIBRARY_PATH='$PROJDIR/sharedlibs' $DEBUGGER ./regionserver -l $CLOCKSERVER -c config${CONFIDX}\"" > /dev/null &
                SSHPROCS="$SSHPROCS $!"
            fi
            echo -n .
            CONFIDX=$[CONFIDX + 1]
            REGIONS_LEFT=$[$REGIONS_LEFT - 1]
        done
        if [ $REGIONS_LEFT -eq 0 ]
        then
            sleep 8
        fi
    elif [ $CLIENTS_LEFT -gt 0 ] && [ $CONTROLLERS -gt 0 ]
    then
        HOSTNUM=`echo $CONTROLHOSTS |wc -w`
        for i in {1..$CLIENTSPERHOST}
        do
          if [ $CLIENTS_LEFT -gt 0 ]
          then
            sleep 0.1
            CLIENTS_LEFT=$[CLIENTS_LEFT - 1]
            CTRLHOST=`echo $CONTROLHOSTS |cut -d ' ' -f $[$CLIENTS_LEFT % $HOSTNUM + 1]`
            CTRLHOST=`host $CTRLHOST|cut -d ' ' -f 4`
            wrap $SSHCOMMAND $HOST "bash -c \"cd '$PROJDIR/client' && LD_LIBRARY_PATH='$PROJDIR/sharedlibs' $DEBUGGER ./client -l $CTRLHOST -t $CLIENTS_LEFT\"" > /dev/null &
            SSHPROCS="$SSHPROCS $!"
          fi
        done
    else
        echo " done!"
        break
    fi
done

if [ $REGIONS_LEFT -ne 0 -o $CONTROLLERS_LEFT -ne 0 ]
then
    echo "Ran out of hosts before we got all servers running!  Shutting down."
    cleanup
    exit 1
fi

echo "All done!  Here's the clock server."
fg 1

cleanup
