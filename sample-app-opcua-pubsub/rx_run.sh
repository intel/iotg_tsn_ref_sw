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

#Launch this script without iperf or vlan:
#	./rx_run.sh <interface> 250000

#Launch this script with iperf and vlan:
#	./rx_run.sh <interface> 250000 iperf_on vlan_on

#Change the gnuplot graphs' center to 300us:
#	./rx_run.sh <interface> 125000 iperf_on vlan_on

#Launch just to replot graphs:
#	./rx_run.sh replot 125000

#	NOTE: DO NOT specify as "enp2s0.vlan"

#Must specify interface
IFACE="$1"
if [ -z $IFACE ]; then
	echo "Error: Please specify interface e.g. './rx_run.sh eth0'"
	exit
fi

#Use default if not specified.
EXP_IFDELAY="$2"
if [ -z $EXP_IFDELAY ]; then
	echo "Error: Please specify midplot point e.g. './rx_run.sh eth0 250000'"
	exit
fi

#Other defaults
NUM_PACKETS=480000	#480000 packets at 250us intervals, represent 2 min of traffic

TX_IP_ADDR="169.254.121.111"
RX_IP_ADDR="169.254.121.222"


# Setup VLAN interfaces
if [ "$3" = "vlan_on" -o "$4" = "vlan_on" ]; then

	VLANCHECK=$(ifconfig | grep $IFACE.vlan | awk 'NR==1{print $3}')

	if [ -z $VLANCHECK ]; then

		echo "Log: Creating TX VLAN interface."

		# Set VLAN ID to 3, all traffic fixed to one VLAN ID, but vary the VLAN Priority
		ip link add link $IFACE name $IFACE.vlan type vlan id 3

		# Provide static ip for VLAN interface as they can't do ARP
		ip addr add $RX_IP_ADDR/24 brd 169.254.255.255 dev $IFACE.vlan

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
		ip neigh add $TX_IP_ADDR dev $IFACE.vlan lladdr $TARGET_MAC_ADDR
		ip neigh show

		# Setup RX parser for VLAN priority 3
		ethtool -N $IFACE flow-type ether vlan 0x0003 vlan-mask 0x1fff action 2

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


#Get the RX/target MAC address. TX should transmit to this address
if [ "$IFACE" != "replot" ]; then
	TARGET_MAC_ADDR=$(ifconfig $IFACE | awk '/HWaddr/ { gsub(/[:]/,"-"); print $5 }')

	echo "Log: Interface: $IFACE"
	echo "Log: MAC Address: $TARGET_MAC_ADDR"
	#NOTE: open62541 APIs require MAC address specified with '-' not ':'


	#Check what terminal emulator to use
	TERM_EMU=$(ps -o comm= -p "$(($(ps -o ppid= -p "$(($(ps -o sid= -p "$$")))")))")
	echo "Log: Using this terminal emulator: $TERM_EMU"

	echo -e "Note: Non-interactive terminals must be closed manually" \
		"\n      and is not the same as normal shell terminals."    \
		"\n      It would only work with a window manager like"     \
		"\n      xfce or gnome."


	#Launch PTP4L
	if pgrep -x "ptp4l" > /dev/null; then
		echo "Log: Already running - ptp4l"
	else
		echo "Log: Launching ptp4l in a new non-interactive terminal."
		$TERM_EMU --command "ptp4l -m -s -2 -i $IFACE" --hold
	fi


	#Launch OPCUA SUBCRIBER
	if pgrep -x "tutorial_pubsub" > /dev/null; then
		echo "Log: Already running - tutorial_pubsub_subscribe"
	else
		echo "Log: Launching tutorial_pubsub_subscribe in a new non-interactive terminal."
		CUR_DIR=$(pwd)
		chmod +x tutorial_pubsub_subscribe
		$TERM_EMU --command "$CUR_DIR/tutorial_pubsub_subscribe opc.eth://$TARGET_MAC_ADDR $IFACE" --hold
	fi


	#Launch IPERF3
	if pgrep -x "iperf3" > /dev/null; then
		echo "Log: Already running - iperf3"
	elif [ "$3" = "iperf_on" -o "$4" = "iperf_on" ]; then
		echo "Log: Launching iperf3 server in a new non-interactive terminal."
		$TERM_EMU --command "iperf3 -s -A 2" --hold
	elif [ "$3" = "iperf_off" -o "$4" = "iperf_off" ]; then
		echo "Log: Skipping - iperf3"
	else
		echo "Log: Skipping - iperf3"
	fi


	#Capture using tcpdump
	if [ "$IFACE" != "replot" ]; then
		echo "Log: Launching tcpdump to capture $NUM_PACKETS ETH UADP packets"
		#tcpdump which filters only 10000 opcua ethernet frames, and calculates their delta
		tcpdump -ttt --time-stamp-precision nano -j adapter_unsynced -B 1512000 -s 128 \
			-e -vv ether proto 0xb62c -i $IFACE \
			-c $NUM_PACKETS -w tcpdump_rxtstamps.pcap
	else
		echo "Log: Replot selected - replotting with existing tcpdump_rxtstamps.pcap"
	fi
fi #replot

#Read and save packets delta
echo "Log: Launching tcpdump to read and parse captured data"
tcpdump -ttt --time-stamp-precision nano -r tcpdump_rxtstamps.pcap \
	| grep 0xb62c \
	| awk '{print $1}' \
	| awk -F. '{print $2}' > extracted_deltas.txt

FILE_LINES=$(stat --print='%s' extracted_deltas.txt)

#Only plot if there is data in the file
if [ $FILE_LINES -ne 0 ]; then
	gnuplot -p -e exp_ifdelay=$EXP_IFDELAY graph_delta_hist.gnu
else
	echo "Log: extracted_deltas.txt is empty. Not plotting."
fi

#Get the first and last timestamps to estimate bandwidth
echo "Estimating bandwidth... "
STARTX=$(tcpdump -r tcpdump_rxtstamps.pcap -tt --time-stamp-precision nano 2>&1 |  grep 0xb62c | head -n 1 | awk '{print $1}')
ENDX=$(tcpdump -r tcpdump_rxtstamps.pcap -tt --time-stamp-precision nano 2>&1 |  grep 0xb62c | tail -1 | awk '{print $1}')

BANDWIDTH=$(awk -v a=$STARTX -v b=$ENDX -v c=$NUM_PACKETS 'BEGIN { print c/(b - a) }')

echo Estimated bandwidth $BANDWIDTH pkt/sec
echo For reference: Inter-packet latency 125us expects 8000pkt/sec

#END
