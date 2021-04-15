#!/bin/sh
#/******************************************************************************
#  Copyright (c) 2020, Intel Corporation
#  All rights reserved.

#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:

#   1. Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.

#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.

#   3. Neither the name of the copyright holder nor the names of its
#      contributors may be used to endorse or promote products derived from
#      this software without specific prior written permission.

#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
# *****************************************************************************/
# Helper functions. This script executes nothing.

###############################################################################
# Defines and defaults
SEC_IN_NSEC=1000000000
NUM_CORE=4

###############################################################################
# PHASE: Iniatialization

set_irq_smp_affinity(){

        IFACE=$1

        AFFINITY_FILE=$2
        if [ -z $AFFINITY_FILE ]; then
                echo "Error: AFFINITY_FILE not defined"; exit 1;
        fi
        echo "Setting IRQ affinity based on $AFFINITY_FILE"

        while IFS=, read -r CSV_Q CSV_CORE CSV_COMMENTS; do
                IRQ_NUM=$(cat /proc/interrupts | grep $IFACE.*$CSV_Q | awk '{print $1}' | tr -d ":")
                echo "Echo-ing 0x$CSV_CORE > /proc/irq/$IRQ_NUM/smp_affinity --> $IFACE:$CSV_Q "
                if [ -z $IRQ_NUM ]; then
                        echo "Error: invalid IRQ NUM"; exit 1;
                fi

                echo $CSV_CORE > /proc/irq/$IRQ_NUM/smp_affinity
        done < $AFFINITY_FILE
}

init_interface(){

        IFACE=$1
        if [ -z $IFACE ]; then 
                echo "Error: please specify interface."
                exit 1
        elif [[ -z $IFACE_MAC_ADDR || -z $IFACE_VLAN_ID ||
                -z $IFACE_IP_ADDR || -z $IFACE_BRC_ADDR ||
                -z $IFACE_VLAN_IP_ADDR || -z $IFACE_VLAN_BRC_ADDR ||
                -z "$VLAN_PRIORITY_SUPPORT" || -z "$VLAN_STRIP_SUPPORT" ||
                -z "$EEE_TURNOFF" || -z "$IRQ_AFFINITY_FILE" ||
                -z $TX_Q_COUNT || -z $RX_Q_COUNT ]]; then
                echo "Source config file first"
                exit 1
        fi

        # Always remove previous qdiscs
        tc qdisc del dev $IFACE parent root 2> /dev/null
        tc qdisc del dev $IFACE parent ffff: 2> /dev/null
        tc qdisc add dev $IFACE ingress

        # Set an even queue pair. Minimum is 4 rx 4 tx.
        if [[ "$RX_Q_COUNT" == "$TX_Q_COUNT" ]]; then
                if [[ $PLAT == i225* ]]; then
                        ethtool -L $IFACE combined $TX_Q_COUNT
                else
                        ethtool -L $IFACE rx $RX_Q_COUNT tx $TX_Q_COUNT
                fi
        else
                echo "Error: use even queue count";
                exit 1
        fi
	
	if [[ $PLAT == i225* ]]; then
		# set to 1Gbps.
		ethtool -s $IFACE advertise 32
		sleep 3
	fi	

        if [[ $PLAT == i225* ]]; then
                RXQ_COUNT=$(ethtool -l $IFACE | sed -e '1,/^Current/d' | grep -i Combined | awk '{print $2}')
                TXQ_COUNT=$RXQ_COUNT
        else
                RXQ_COUNT=$(ethtool -l $IFACE | sed -e '1,/^Current/d' | grep -i RX | awk '{print $2}')
                TXQ_COUNT=$(ethtool -l $IFACE | sed -e '1,/^Current/d' | grep -i TX | awk '{print $2}')
        fi

        if [[ "$RXQ_COUNT" != "$TXQ_COUNT" ]]; then
                echo "Error: TXQ and RXQ count do not match."; exit 1;
        fi

        # Make sure systemd do not manage the interface
        mv /lib/systemd/network/80-wired.network . 2> /dev/null

        # Restart interface and systemd, also set HW MAC address for multicast
        ip link set $IFACE down
        systemctl restart systemd-networkd.service
        ip link set dev $IFACE address $IFACE_MAC_ADDR
        ip link set dev $IFACE up
        sleep 3

        # Set VLAN ID to 3, all traffic fixed to one VLAN ID, but vary the VLAN Priority
        ip link delete dev $IFACE.vlan 2> /dev/null
        ip link add link $IFACE name $IFACE.vlan type vlan id $IFACE_VLAN_ID

        # Provide static ip address for interfaces
        ip addr flush dev $IFACE
        ip addr flush dev $IFACE.vlan
        ip addr add $IFACE_IP_ADDR/24 brd $IFACE_BRC_ADDR dev $IFACE
        ip addr add $IFACE_VLAN_IP_ADDR/24 brd $IFACE_VLAN_BRC_ADDR dev $IFACE.vlan

        # Map socket priority N to VLAN priority N
        if [[ "$VLAN_PRIORITY_SUPPORT" == "YES" ]]; then
                echo "Mapping socket priority N to VLAN priority N for $IFACE"
                ip link set $IFACE.vlan type vlan egress-qos-map 1:1
                ip link set $IFACE.vlan type vlan egress-qos-map 2:2
                ip link set $IFACE.vlan type vlan egress-qos-map 3:3
                ip link set $IFACE.vlan type vlan egress-qos-map 4:4
                ip link set $IFACE.vlan type vlan egress-qos-map 5:5
                ip link set $IFACE.vlan type vlan egress-qos-map 6:6
                ip link set $IFACE.vlan type vlan egress-qos-map 7:7
        fi

        # Flush neighbours, just in case
        ip neigh flush all dev $IFACE
        ip neigh flush all dev $IFACE.vlan

        # Turn off VLAN Stripping
        if [[ "$VLAN_STRIP_SUPPORT" == "YES" ]]; then
                echo "Turning off vlan stripping"
                ethtool -K $IFACE rxvlan off
        fi

        # Disable EEE option is set in config file
        if [[ "$EEE_TURNOFF" == "YES" ]]; then
                echo "Turning off EEE"
                ethtool --set-eee $IFACE eee off &> /dev/null
        fi

        # Set irq affinity
        set_irq_smp_affinity $IFACE $DIR/../common/$IRQ_AFFINITY_FILE
}

###############################################################################
# PHASE: Setup

setup_taprio(){
        local CMD;
        IFACE=$1

        if [ -z $IFACE ]; then echo "Error: please specify interface."; exit 1; fi

        if [[ -z $TAPRIO_MAP || -z $TAPRIO_SCHED ]]; then
                echo "Error: TAPRIO vars not defined"; exit 1;
        fi

        # # To use replace, we need a base for the first time. Also, we want to
        # # ensure no packets are "stuck" in a particular queue if TAPRIO completely
        # # closes it off.
        # # This command is does nothing if used when there's an existing qdisc.
        tc qdisc add dev $IFACE root mqprio \
                num_tc 1 map 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \
                queues 1@0 hw 0 &> /dev/null

        sleep 5

        #Count
        TAPRIO_ARR=($TAPRIO_MAP)
        NUM_TC=$(printf "%s\n" ${TAPRIO_ARR[@]} | sort | tail -n 1)

        for i in $(seq 0 $NUM_TC); do
                QUEUE_OFFSETS="$QUEUE_OFFSETS 1@$i"
        done

        NUM_TC=$(expr $NUM_TC + 1)

        # i225 does not support basetime in the future
        if [[ $PLAT == i225* ]]; then
                BASE=$(date +%s%N)
        else
                BASE=$(expr $(date +%s) + 5)000000000
        fi

        CMD=$(echo "tc qdisc replace dev $IFACE parent root handle 100 taprio" \
                "num_tc $NUM_TC map $TAPRIO_MAP" \
                "queues $QUEUE_OFFSETS " \
                "base-time $BASE" \
                "${TAPRIO_SCHED[@]}" \
                "$TAPRIO_FLAGS")

        echo "Run: $CMD"; $CMD;
}

setup_mqprio(){
        local CMD;
        IFACE=$1

        if [ -z $IFACE ]; then echo "Error: please specify interface."; exit 1; fi

        if [ -z "$MQPRIO_MAP" ]; then
                echo "Error: MQPRIO_MAP not defined"; exit 1;
        fi

        #Count
        MQPRIO_ARR=($MQPRIO_MAP)
        NUM_TC=$(printf "%s\n" ${MQPRIO_ARR[@]} | sort | tail -n 1)

        for i in $(seq 0 $NUM_TC); do
                QUEUE_OFFSETS="$QUEUE_OFFSETS 1@$i"
        done

        NUM_TC=$(expr $NUM_TC + 1)

        CMD=$(echo "tc qdisc replace dev $IFACE parent root handle 100 mqprio" \
                " num_tc $NUM_TC map $MQPRIO_MAP queues $QUEUE_OFFSETS" \
                " hw 0")

        echo "Run: $CMD"; $CMD;
}

setup_etf(){
        #TODO: Supports specifying 1ETF Q per port only,
        #      add a for loop and array if need more
        IFACE=$1

        if [ -z $IFACE ]; then echo "Error: please specify interface."; exit 1; fi

        if [[ -z $ETF_Q  || -z $ETF_DELTA ]]; then
                echo "Error: ETF_q or ETF_DELTA not specified"; exit 1;
        fi

        NORMAL_QUEUE=$(expr $ETF_Q + 1) #TC qdisc id start from 1 instead of 0

        #ETF qdisc
        HANDLE_ID="$( tc qdisc show dev $IFACE | tr -d ':' | awk 'NR==1{print $3}' )"

        # The ETF_DELTA dont really apply to AF_XDP.

        CMD=$(echo "tc qdisc replace dev $IFACE parent $HANDLE_ID:$NORMAL_QUEUE etf" \
                " clockid CLOCK_TAI delta $ETF_DELTA offload" \
                " $ETF_FLAGS") #deadline_mode off skip_sock_mode off

        echo "Run: $CMD"; $CMD;
}

# For setup phase
setup_vlanrx(){
        local CMD;
        IFACE=$1

        if [ -z $IFACE ]; then echo "Error setup_vlanrx: please specify interface."; exit 1; fi

        if [ -z "$VLAN_RX_MAP" ]; then
                echo "Warning: VLAN_RX_MAP not defined. Vlan rx queue steering is NOT set."; exit 1;
        fi

        tc qdisc del dev $IFACE ingress
        tc qdisc add dev $IFACE ingress

        NUM_ENTRY=$(expr ${#VLAN_RX_MAP[@]} - 1)
        for i in $(seq 0 $NUM_ENTRY ); do
                CMD=$(echo "tc filter add dev $IFACE parent ffff:" \
                           " protocol 802.1Q flower ${VLAN_RX_MAP[$i]}")
                echo "Run: $CMD"; $CMD;
        done

        tc filter show dev $IFACE ingress
}

setup_vlanrx_xdp(){
        local CMD;
        IFACE=$1

        if [ -z $IFACE ]; then echo "Error setup_vlanrx_xdp: please specify interface."; exit 1; fi

        if [ -z "$VLAN_RX_MAP_XDP" ]; then
                echo "Warning: VLAN_RX_MAP for XDP not defined. Vlan rx queue steering XDP is NOT set."; exit 1;
        fi

        tc qdisc del dev $IFACE ingress
        tc qdisc add dev $IFACE ingress

        NUM_ENTRY=$(expr ${#VLAN_RX_MAP_XDP[@]} - 1)
        for i in $(seq 0 $NUM_ENTRY ); do
                CMD=$(echo "tc filter add dev $IFACE parent ffff:" \
                           " protocol 802.1Q flower ${VLAN_RX_MAP_XDP[$i]}")
                echo "Run: $CMD"; $CMD;
        done

        tc filter show dev $IFACE ingress
}

enable_extts(){
        IFACE=$1

        if [ -z $1 ]; then
                echo "Please enter interface: ./enable_extts.sh iface"; exit 1;
        fi

        CLK=`ethtool -T $IFACE | grep -Po "(?<=PTP Hardware Clock: )[\d+]"`
        PCLK=ptp$CLK
        echo "Enabling extts on $IFACE ($PCLK)"

        if [[ $PLAT == i225* ]]; then
                #Set the SDP1 to input for extts
                echo 1 0 > /sys/class/ptp/$PCLK/pins/SDP1
        fi

        # enable ext timestamping
        echo 0 1 > /sys/class/ptp/$PCLK/extts_enable
}

enable_pps(){
        if [ -z $1 ]; then
                echo "Please enter interface: ./setup_pps.sh iface"
                exit
        fi

        IFACE=$1

        CLK=`ethtool -T $IFACE | grep -Po "(?<=PTP Hardware Clock: )[\d+]"`
        PCLK=ptp$CLK
        echo "Enabling pps on $IFACE ($PCLK)"

        if [[ $PLAT == i225* ]]; then
                 #Set the SDP0 as PPS output
                echo 2 0 > /sys/class/ptp/$PCLK/pins/SDP0
        fi

        # configure pps
        # echo <idx> <ts> <tns> <ps> <pns> > /sys/class/ptp/ptpX/period
        # idx -> PPS number
        # ts  -> start time (second), based on ptp time
        # tns -> start time (nano-second), based on ptp time
        # ps  -> period (s)
        # pns -> period (ns)
        # ptpX -> is ptp of ethernet interface
        echo 0 0 0 1 0 > /sys/class/ptp/$PCLK/period
}

###############################################################################
# PHASE: Runtime

run_iperf3_bg_client(){

        if [ -z $TARGET_IP_ADDR ]; then echo "Error: please specify target ip."; exit 1; fi

        #Run Iperf3
        #    Send to ip
        #    UDP
        #    bandwidth 200Mbps
        #    size of packets 1440 bytes
        #    output format in Mbits/sec
        #    output interval 10s
        #    run for 30000s
        #     always use CPU0, general purpose core

        if [ ! -z "$SSH_CLIENT" ]; then
                # Headless mode, use ssh root@127.0.0.1 to trigger loopback if required
                iperf3 -c $TARGET_IP_ADDR -u -b $IPERF_BITRATE -l 1440 -f m -i 10 -t 30000 -A $CPU_AFFINITY &
        else
                # UI, X-server mode. Opens dedicated window
               xterm -e 'iperf3 -c 169.254.1.22 -u -b 200M -l 1440 -f m -i 10 -t 30000 -A 0' &
        fi

        if [ ! -z "$TARGET_IP_ADDR2" ]; then
                iperf3 -c $TARGET_IP_ADDR2 -u -b $IPERF_BITRATE -l 1440 -f m -i 10 -t 30000 -A $CPU_AFFINITY &
        fi
}

run_iperf3_bg_server(){

        if [ -z $IFACE_IP_ADDR ]; then echo "Error: specify ip to listen for."; exit 1; fi

        #Run Iperf3
        #     run server
        #     bind to ip
        #     output interval 10s
        #     always use CPU0, general purpose core

        if [ ! -z "$SSH_CLIENT" ]; then
                # Headless mode, use ssh root@127.0.0.1 to trigger loopback if required
                iperf3 -s -B $IFACE_IP_ADDR -i 10 -1 -A $CPU_AFFINITY &
        else
                # UI, X-server mode. Opens dedicated window
                xterm -e 'iperf3 -s -B 169.254.1.22 -i 10 -A 0 -1' &
        fi
}

###############################################################################
# For Results calculation phase

calc_rx_u2u(){
        local RX_FILENAME=$1 #*-rxtstamps.txt
        SHORTNAME=$(echo $RX_FILENAME | awk -F"-" '{print $1}')

        #U2U stats
        cat $RX_FILENAME | \
                awk '{ print $1 "\t" $2}' | \
                awk '{ if($2 > 0){
                        if(min==""){min=max=$1};
                        if($1>max) {max=$1};
                        if($1<min) {min=$1};
                        total+=$1; count+=1
                        }
                      } END {print "U2U\t"max,"\t"total/count,"\t"min}' > temp1.txt

        echo -e "Results\tMax\tAvg\tMin" > temp0.txt
        column -t temp0.txt temp1.txt > saved1.txt
        rm temp*.txt

        #Plot u2u
        cat $RX_FILENAME | \
                         awk '{ print $1 "\t" $2 }' > $SHORTNAME-traffic.txt
}

calc_rx_duploss(){
        local XDP_RX_FILENAME=$1 #*-txtstamps.txt

        # Total packets
        cat $XDP_RX_FILENAME \
                | awk '{print $2}' \
                | grep -x -E '[0-9]+' \
                | wc -l > temp1.txt

        # Total duplicate
        cat $XDP_RX_FILENAME \
                | awk '{print $2}' \
                | uniq -D \
                | wc -l | paste temp1.txt - > temp2.txt

        # Total missing: Same as: total_packet - total_unique (uniq -c)
        cat $XDP_RX_FILENAME \
                | awk '{print $2}' \
                | grep -x -E '[0-9]+' \
                | awk '{for(i=p+1; i<$1; i++) print i} {p=$1}' \
                | wc -l | paste temp2.txt - > temp3.txt

        echo -e "Total\tDuplicates\tLosses" > temp0.txt
        cat temp3.txt >> temp0.txt
        paste saved1.txt temp0.txt | column -t

        rm temp*.txt
}

stop_if_empty(){
        wc -l $1 > /dev/null
        if [ $? -gt 0 ]; then
                exit
        fi

        COUNT=$(wc -l $1 | awk '{print $1}')

        if [ $COUNT -lt 3000 ]; then
                echo "Too little data $1 has only $COUNT lines"
                exit
        fi
        echo "$1 has $COUNT lines"
}

save_result_files(){

        ID=$(date +%Y%m%d)
        IDD=$(date +%Y%m%d-%H%M)
        mkdir -p results-$ID

        rm -f plot_pic.png #remove existing plot files.

        CONFIG=$1
        NUMPKTS=$2
        SIZE=$3
        INTERVAL=$4
        XDP_MODE=$5
        PLAT=$6

        case "$CONFIG" in

        vs1b)
                if [[ "$XDP_MODE" != "NA" ]]; then
                    cp afxdp-rxtstamps.txt results-$ID/$PLAT-afxdp-$CONFIG-$NUMPKTS-$SIZE-$XDP_INTERVAL-rxtstamps-$IDD.txt
                fi
                cp afpkt-rxtstamps.txt results-$ID/$PLAT-afpkt-$CONFIG-$NUMPKTS-$SIZE-$INTERVAL-rxtstamps-$IDD.txt
        ;;

        *)
                echo "Error: save_results_files() invalid config: $CONFIG"
                exit
        ;;
        esac
}
