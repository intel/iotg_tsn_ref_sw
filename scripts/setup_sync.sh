INTERFACE='NULL'
BOARD='NULL'
DAEMONCL=0

function start_daemoncl {
	local _command="daemon_cl $1 -R 1 -D 0,0,0,0"
	if [ $2 == "boardB" ]
	then
		_command="daemon_cl $1 -S -D 0,0,0,0"
	fi

	_command="$_command &"
	echo $_command
}
function start_ptp4l {
	# default value for boardA
	local _command="ptp4l -i $1 -A -2 -m"
	if [ $2 == "boardB" ]
	then
		_command="$_command -s"
	fi

	# run as detached
	_command="$_command &"
	# return command
	echo $_command
}

function start_phc2sys {
	# default value for boardA
	local phc2sys_s="CLOCK_REALTIME"
	local phc2sys_c=$1
	if [ $2 == "boardB" ]
	then
		phc2sys_s=$1
		phc2sys_c="CLOCK_REALTIME"
	fi

	# return command
	local _command="phc2sys -s $phc2sys_s -c $phc2sys_c -w -m -O 0 &"
	echo $_command
}

function new_terminal {
	xfce4-terminal --hide-menubar --hide-toolbar -T $1 -e "$2"
}

function usage {
	echo "Usage:"
	echo "./setup_sync.sh [-h] -i interface -b {boardA|boardB} [-d]"
	echo
	echo "-b boardA or boardB"
	echo "-d use daemon_cl instead of ptp4l & phc2sys"
	echo "-h show this help"
	echo "-i network interface"
	exit -1
}

# handle command line
while getopts b:dhi: opt; do
	case ${opt} in
		b) BOARD=${OPTARG} ;;
		d) DAEMONCL=1 ;;
		i) INTERFACE=${OPTARG} ;;
		*) help=1 ;;
	esac
done

# check & display usage
if [ -v help ]
then
	usage
fi


# variable check
if [ ${BOARD} == 'NULL' ]
then
	usage
fi

# variable check
if [ ${INTERFACE} == 'NULL' ]
then
	usage
fi

# check if board name are valid
if [ "$BOARD" == "boardA" ] || [ "$BOARD" == "boardB" ]
then
	true
else
	echo "board $BOARD is not valid."
	exit -1
fi

# check if interface are valid
if [ ! -d /sys/class/net/$INTERFACE ]
then
	echo "interface $INTERFACE is not available."
	exit -1
fi

# execute command
if [ $DAEMONCL -gt 0 ]
then
	new_terminal SYNC-DAEMONCL "$(start_daemoncl $INTERFACE $BOARD)"
	echo 'daemon_cl started.'
else
	new_terminal SYNC-PTP4L "$(start_ptp4l $INTERFACE $BOARD)"
	echo 'ptp4l started.'
	echo 'Press Enter to start phc2sys.'
	read
	new_terminal SYNC-PHC2SYS "$(start_phc2sys $INTERFACE $BOARD)"
	echo 'phc2sys started.'
fi
