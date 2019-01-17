INTERFACE='NULL'
BOARD='NULL'
VLAN=0

function set_realtime_thread {
	sysctl kernel.sched_rt_runtime_us=-1
}

function set_fixed_ip {
	echo "flush IP & setting fixed IP."
	ip addr flush dev $1
	sleep 3
	if [ $2 == "boardA" ]
	then
		ip addr add 169.254.0.1/24 brd 169.254.0.255 dev $1
	elif [ $2 == "boardB" ]
	then
		ip addr add 169.254.0.2/24 brd 169.254.0.255 dev $1
	fi
}

function set_virtual_interface_vlan {
	echo "checking if virtual interface is enabled."
	if [ $(ip link show $1.3 | grep -c $1.3) -gt 0 ]
	then
		echo "VLAN interface already enabled."
	else
		echo "enabling virtual interface VLAN $1.3"
		if [ $2 == "boardA" ]
		then
			ip link add link $1 name $1.3 type vlan id 3 egress-qos-map 7:7
			ip addr show $1 && ip addr show $1.3
		elif [ $2 == "boardB" ]
		then
			ip link add link $1 name $1.3 type vlan id 3
			ip addr show $1 && ip addr show $1.3
		fi
		echo 'virtual interface VLAN enabled'
	fi
}

function usage {
	echo "Usage:"
	echo "./setup_generic.sh [-h] -i interface -b {boardA|boardB} [-v]"
	echo "-h    show this help"
	echo "-i    network interface"
	echo "-v    setup virtual interface for VLAN"
	exit -1
}

# handle command line
while getopts b:hi:v opt; do
	case ${opt} in
		b) BOARD=${OPTARG} ;;
		i) INTERFACE=${OPTARG} ;;
		v) VLAN=1 ;;
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
set_fixed_ip $INTERFACE $BOARD
echo 'fixed-ip enabled.'
if [ $VLAN -gt 0 ]
then
	set_virtual_interface_vlan $INTERFACE $BOARD
fi
if [ "$BOARD" == "boardA" ]
then
	set_realtime_thread
	echo 'realtime thread enabled.'
fi
