###############################################################################
#
# Copyright (c) 2019, Intel Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
#  3. Neither the name of the copyright holder nor the names of its
#     contributors may be used to endorse or promote products derived from
#     this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
###############################################################################

#Launch this script with mqprio:
#	./tx_run.sh <interface> <profile>
#	E.g.	./tx_run.sh enp2s0 1

#Use vlan:
#	./tx_run.sh <interface> <profile>
#	E.g.	./tx_run.sh enp2s0 1 vlan_on
#	NOTE: DO NOT specify as "enp2s0.vlan"

#Must specify interface
IFACE="$1"
if [ -z $IFACE ]; then
	echo "Error: Please specify interface e.g. './tx_run.sh enp2s0 1'"
	exit
fi


PROFILE="$2"
if [ -z $PROFILE ]; then
	echo -e "Error: Please specify profile e.g. './tx_run.sh enp2s0 3'" \
		"\n 0	NONE       Do not setup qdisc, only re-run publisher" \
		"\n 1	MQPRIO     Route packets to queues." \
		"\n 2	MQPRIO_ETF Route packets to queues where 1 queue is ETF qdisc" \
		"\n 3	TAPRIO     Use TAPRIO qdisc with schedule " \
		"\n 4	TAPRIO_ETF Use TAPRIO qdisc with schedule and ETF qdisc "
	exit
fi

#Target ip is for VLAN use only, Target MAC is for publishing
TX_IP_ADDR="169.254.121.111"
RX_IP_ADDR="169.254.121.222"
RX_MAC_ADDR="98-4F-EE-0C-71-C9"
echo "Log: Using target MAC Addr: $RX_MAC_ADDR"
echo "Log: Using target IP Addr: $RX_IP_ADDR"



# TODO: decide if we should remove this for sch_dl
# Set to use Real-time scheduler, suppress output.
sysctl kernel.sched_rt_runtime_us=-1 > /dev/null

# Set to use performance mode
NCPUS=$(cat /proc/cpuinfo | grep processor | wc -l)
for ((i=0; i < $NCPUS; i++)) do
	echo performance > /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor
done
sleep 1


if [ $PROFILE -gt 0 ] && [ $PROFILE -lt 5 ]; then

	echo "Log: Selected profile: $PROFILE"

	case $PROFILE in
	1) #MQPRIO
		python scheduler.py -i $IFACE -q queue-s1.cfg -e 3
	;;
	2) #MQPRIO_ETF #TODO: move to python scheduler.py
		tc qdisc del dev $IFACE root
		tc qdisc replace dev $IFACE parent root mqprio  \
			num_tc 3 map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2    \
			queues 1@0 1@1 2@2 hw 0

		# Get the handle id of the qdisc of this interface, must be after mqprio/taprio
		HANDLE_ID="$( tc qdisc show dev $IFACE | tr -d ':' | awk 'NR==1{print $3}' )"

		# Set the ETF qdisc with 5000000 delta time
		tc qdisc replace dev $IFACE parent $HANDLE_ID:1 etf         \
			offload clockid CLOCK_TAI delta 5000000
	;;
	3) #TAPRIO
		python scheduler.py -i $IFACE -q queue-s2.cfg -e 3 -g gates-s4.sched
	;;
	4) #TAPRIO ETF
		python scheduler.py -i $IFACE -q queue-s3s4.cfg -e 3 -g gates-s4.sched
	;;
	esac

	#Wait for the RX-TX to re-establish connection
	echo -e "Log: TC commands will reset network interface. Wait 30 secs..."
	sleep 30
elif [ "$2" == 0 ]; then
	echo "Log: Skipping tc qdisc setup."
else
	echo -e "Error: Please specify profile e.g. './tx_run.sh enp2s0 3'" \
		"\n 0   NONE       Do not setup qdisc, only re-run publisher" \
		"\n 1   MQPRIO     Route packets to queues." \
		"\n 2   MQPRIO_ETF Route packets to queues where 1 queue is ETF qdisc" \
		"\n 3   TAPRIO     Use TAPRIO qdisc with schedule " \
		"\n 4   TAPRIO_ETF Use TAPRIO qdisc with schedule and ETF qdisc "
	exit
fi


# Setup VLAN interfaces
if [ "$3" = "vlan_on" -o "$4" = "vlan_on" ]; then

	VLANCHECK=$(ifconfig | grep $IFACE.vlan | awk 'NR==1{print $3}')

	if [ -z $VLANCHECK ]; then

		echo "Log: Creating TX VLAN interface."

		# Set VLAN ID to 3, all traffic fixed to one VLAN ID, but vary the VLAN Priority
		ip link add link $IFACE name $IFACE.vlan type vlan id 3

		# Provide static ip for VLAN interface as they can't do ARP
		ip addr add $TX_IP_ADDR/24 brd 169.254.255.255 dev $IFACE.vlan

		# VLAN priority is used by switches or a RX interface's "rx-steering/filtering"
		# feature to steer packets into dedicated RX queue.
		# Turn on the VLAN interface and launch app into $IFACE.vlan to send with VLAN tagging.
		ip link set dev $IFACE.vlan up

		# TODO: check if this is socket priority or traffic class

		# Map socket priority 3 to VLAN priority 5
		ip link set $IFACE.vlan type vlan egress 3:5

		# Map socket priority 1 to VLAN priority 0
		ip link set $IFACE.vlan type vlan egress 1:0

		# Clear and add RX's static ip to TX's ARP table
		ip neigh flush all dev $IFACE.vlan
		ip neigh add $RX_IP_ADDR dev $IFACE.vlan lladdr $RX_MAC_ADDR
		ip neigh show

		IFACE=$IFACE.vlan
	else
		echo "Log: VLAN interface exists. Skipping VLAN setup."
		IFACE=$IFACE.vlan
	fi

elif [ "$3" = "vlan_off" -o "$4" = "vlan_off" ]; then
	echo "Log: Removing VLAN interfaces. Please restart on another interface"
	ip link del $IFACE.vlan
	exit
else
	echo "Log: Skipping VLAN setup."
fi #VLAN enablement


#Check what terminal emulator to use to open new (non-interactive) terminals
TERM_EMU=$(ps -o comm= -p "$(($(ps -o ppid= -p "$(($(ps -o sid= -p "$$")))")))")
echo "Log: Non-interactive terminals will use: $TERM_EMU"
echo -e "Note: Non-interactive terminals must be closed manually" \
	"\n      and is not the same as normal shell terminals."    \
	"\n      It would only work when run in a window manager like"     \
	"\n      xfce or gnome. "

#Launch PTP4L
if pgrep -x "ptp4l" > /dev/null; then
	echo "Log: Already running - ptp4l"
else
	echo "Log: Launching ptp4l in a new non-interactive terminal. Allow 30s for PTP to sync.."
	$TERM_EMU --command "ptp4l -m -2 -i $IFACE" --hold
	sleep 30 #let it sync first
fi

#Launch PHC2SYS
if pgrep -x "phc2sys" > /dev/null; then
	echo "Log: Already running - phc2sys"
else
	echo "Log: Launching phc2sys in a new non-interactive terminal."
	$TERM_EMU --command "phc2sys -O 0 -c CLOCK_REALTIME -w -m -s $IFACE" --hold
fi

# TODO: fix nohup
# #Launch IPERF3
# if pgrep -x "iperf3" > /dev/null; then
# 	echo "Log: Already running - iperf3"
# elif [ "$3" = "iperf_on" -o "$4" = "iperf_on" ]; then
# 	echo "Log: Launching iperf3 client in a new non-interactive terminal."
# 	$TERM_EMU --command "nohup iperf3 -t 600 -b 0 -u -l 1448 -A 2 -c $RX_IP_ADDR " --hold
# elif [ "$3" = "iperf_off" -o "$4" = "iperf_off" ]; then
# 	echo "Log: Skipping - iperf3"
# else
# 	echo "Log: Skipping - iperf3"
# fi


#Launch OPCUA PUBLISHER
if pgrep -x "tutorial_pubsub" > /dev/null; then
	echo "Log: Already running - tutorial_pubsub_publish"
else
	echo "Log: Launching tutorial_pubsub_publish in THIS terminal."
	chmod +x tutorial_pubsub_publish
	./tutorial_pubsub_publish opc.eth://$RX_MAC_ADDR $IFACE
fi

#END
