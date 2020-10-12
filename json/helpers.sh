#!/bin/bash
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
# TODO: Move these into a python script, perhaps launch.py?

set_irq_smp_affinity(){

        IFACE=$1
        AFFINITY_FILE=$2

        # echo "Setting IRQ affinity based on $AFFINITY_FILE"
        # echo "Note: affinity file should have empty new line at the end."

        #Rather than mapping all irqs to a CPU, we only map the queues we use
        # via the affinity file. Only 2 columns used, last column is comments
        while IFS=, read -r CSV_Q CSV_CORE CSV_COMMENTS
        do
                IRQ_NUM=$(cat /proc/interrupts | grep $IFACE:$CSV_Q | awk '{print $1}' | rev | cut -c 2- | rev)
                # echo "Echo-ing 0x$CSV_CORE > /proc/irq/$IRQ_NUM/smp_affinity $IFACE:$CSV_Q "
                echo $CSV_CORE > /proc/irq/$IRQ_NUM/smp_affinity
        done < $AFFINITY_FILE
}

setup_sp1a(){
        # Static IP and MAC addresses are hardcoded here

        IFACE=$1

        # Always remove previous qdiscs
        tc qdisc del dev $IFACE parent root 2> /dev/null
        tc qdisc del dev $IFACE parent ffff: 2> /dev/null
        tc qdisc add dev $IFACE ingress

        RXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==3{ print $2}')
        TXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==4{ print $2}')

        if [ $RXQ_COUNT != $TXQ_COUNT ]; then
                # Set it to even queue count. Minimum is 4 rx 4 tx.
                ethtool -L $IFACE rx 4 tx 4
        fi

        # Make sure systemd do not manage the interface
        mv /lib/systemd/network/80-wired.network . 2> /dev/null

        # Restart interface and systemd, also set HW MAC address for multicast
        ip link set $IFACE down
        systemctl restart systemd-networkd.service
        ip link set dev $IFACE address aa:00:aa:00:aa:00
        ip link set dev $IFACE up
        sleep 3

        # Set VLAN ID to 3, all traffic fixed to one VLAN ID, but vary the VLAN Priority
        ip link delete dev $IFACE.vlan 2> /dev/null
        ip link add link $IFACE name $IFACE.vlan type vlan id 3

        # Provide static ip address for interfaces
        ip addr flush dev $IFACE
        ip addr flush dev $IFACE.vlan
        ip addr add 169.254.1.11/24 brd 169.254.1.255 dev $IFACE
        ip addr add 169.254.11.11/24 brd 169.254.11.255 dev $IFACE.vlan

        # Map socket priority N to VLAN priority N
        ip link set $IFACE.vlan type vlan egress-qos-map 1:1
        ip link set $IFACE.vlan type vlan egress-qos-map 2:2
        ip link set $IFACE.vlan type vlan egress-qos-map 3:3
        ip link set $IFACE.vlan type vlan egress-qos-map 4:4
        ip link set $IFACE.vlan type vlan egress-qos-map 5:5

        # Flush neighbours, just in case
        ip neigh flush all dev $IFACE
        ip neigh flush all dev $IFACE.vlan

        if [ $TXQ_COUNT -eq 8 ]; then
                set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_8tx_8rx.map
        else
                set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_4tx_4rx.map
        fi

        # OPCUA-PKT3? multistream
}

setup_sp2a(){
        # Static IP and MAC addresses are hardcoded here

        IFACE=$1

        # Always remove previous qdiscs
        tc qdisc del dev $IFACE parent root 2> /dev/null
        tc qdisc del dev $IFACE parent ffff: 2> /dev/null
        tc qdisc add dev $IFACE ingress

        RXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==3{ print $2}')
        TXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==4{ print $2}')

        if [ $RXQ_COUNT != $TXQ_COUNT ]; then
                # Set it to even queue count. Minimum is 4 rx 4 tx.
                ethtool -L $IFACE rx 4 tx 4
        fi

        # Restart interface and systemd, also set HW MAC address for multicast
        ip link set $IFACE down
        ip link set dev $IFACE address aa:11:aa:11:aa:11
        ip link set dev $IFACE up
        sleep 3

        # Set VLAN ID to 3, all traffic fixed to one VLAN ID, but vary the VLAN Priority
        ip link delete dev $IFACE.vlan 2> /dev/null
        ip link add link $IFACE name $IFACE.vlan type vlan id 3

        # Provide static ip address for interfaces
        ip addr flush dev $IFACE
        ip addr flush dev $IFACE.vlan
        ip addr add 169.254.2.11/24 brd 169.254.2.255 dev $IFACE
        ip addr add 169.254.22.11/24 brd 169.254.22.255 dev $IFACE.vlan

        # Map socket priority N to VLAN priority N
        ip link set $IFACE.vlan type vlan egress-qos-map 1:1
        ip link set $IFACE.vlan type vlan egress-qos-map 2:2
        ip link set $IFACE.vlan type vlan egress-qos-map 3:3
        ip link set $IFACE.vlan type vlan egress-qos-map 4:4
        ip link set $IFACE.vlan type vlan egress-qos-map 5:5

        # Flush neighbours, just in case
        ip neigh flush all dev $IFACE
        ip neigh flush all dev $IFACE.vlan

        if [ $TXQ_COUNT -eq 8 ]; then
                set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_8tx_8rx-multi.map
        else
                set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_4tx_4rx.map
        fi
}


setup_sp1b(){
        # Static IP and MAC addresses are hardcoded here

        IFACE=$1

        # Always remove previous qdiscs
        tc qdisc del dev $IFACE parent root 2> /dev/null
        tc qdisc del dev $IFACE parent ffff: 2> /dev/null
        tc qdisc add dev $IFACE ingress

        RXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==3{ print $2}')
        TXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==4{ print $2}')

        if [ $RXQ_COUNT != $TXQ_COUNT ]; then
                # Set it to even queue count. Minimum is 4 rx 4 tx.
                ethtool -L $IFACE rx 4 tx 4
        fi

        # Make sure systemd do not manage the interface
        mv /lib/systemd/network/80-wired.network . 2> /dev/null

        # Restart interface and systemd, also set HW MAC address for multicast
        ip link set $IFACE down
        systemctl restart systemd-networkd.service
        ip link set dev $IFACE address 22:bb:22:bb:22:bb
        ip link set dev $IFACE up
        sleep 3

        # Set VLAN ID to 3, all traffic fixed to one VLAN ID, but vary the VLAN Priority
        ip link delete dev $IFACE.vlan 2> /dev/null
        ip link add link $IFACE name $IFACE.vlan type vlan id 3

        # Provide static ip address for interfaces
        ip addr flush dev $IFACE
        ip addr flush dev $IFACE.vlan
        ip addr add 169.254.1.22/24 brd 169.254.1.255 dev $IFACE
        ip addr add 169.254.11.22/24 brd 169.254.11.255 dev $IFACE.vlan

        # Map socket priority N to VLAN priority N
        ip link set $IFACE.vlan type vlan egress-qos-map 1:1
        ip link set $IFACE.vlan type vlan egress-qos-map 2:2
        ip link set $IFACE.vlan type vlan egress-qos-map 3:3
        ip link set $IFACE.vlan type vlan egress-qos-map 4:4
        ip link set $IFACE.vlan type vlan egress-qos-map 5:5

        # Flush neighbours, just in case
        ip neigh flush all dev $IFACE
        ip neigh flush all dev $IFACE.vlan

        if [ $TXQ_COUNT -eq 8 ]; then
                set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_8tx_8rx.map
        else
                set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_4tx_4rx.map
        fi
}

setup_sp2b(){
        # Static IP and MAC addresses are hardcoded here

        IFACE=$1

        # Always remove previous qdiscs
        tc qdisc del dev $IFACE parent root 2> /dev/null
        tc qdisc del dev $IFACE parent ffff: 2> /dev/null
        tc qdisc add dev $IFACE ingress

        RXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==3{ print $2}')
        TXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==4{ print $2}')

        if [ $RXQ_COUNT != $TXQ_COUNT ]; then
                # Set it to even queue count. Minimum is 4 rx 4 tx.
                ethtool -L $IFACE rx 4 tx 4
        fi

        # Restart interface and systemd, also set HW MAC address for multicast
        ip link set $IFACE down
        ip link set dev $IFACE address 22:cc:22:cc:22:cc
        ip link set dev $IFACE up
        sleep 3

        # Set VLAN ID to 3, all traffic fixed to one VLAN ID, but vary the VLAN Priority
        ip link delete dev $IFACE.vlan 2> /dev/null
        ip link add link $IFACE name $IFACE.vlan type vlan id 3

        # Provide static ip address for interfaces
        ip addr flush dev $IFACE
        ip addr flush dev $IFACE.vlan
        ip addr add 169.254.2.22/24 brd 169.254.22.255 dev $IFACE
        ip addr add 169.254.22.22/24 brd 169.254.22.255 dev $IFACE.vlan

        # Map socket priority N to VLAN priority N
        ip link set $IFACE.vlan type vlan egress-qos-map 1:1
        ip link set $IFACE.vlan type vlan egress-qos-map 2:2
        ip link set $IFACE.vlan type vlan egress-qos-map 3:3
        ip link set $IFACE.vlan type vlan egress-qos-map 4:4
        ip link set $IFACE.vlan type vlan egress-qos-map 5:5

        # Flush neighbours, just in case
        ip neigh flush all dev $IFACE
        ip neigh flush all dev $IFACE.vlan

        if [ $TXQ_COUNT -eq 8 ]; then
                set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_8tx_8rx-multi.map
        else
                set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_4tx_4rx.map
        fi
}


calc_rx_u2u(){
        # Output format (1 way):
        # latency, rx_sequence, sdata->id, txTime, RXhwTS, rxTime

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
                      } END {print "U2U\n"total/count,"\n"max,"\n"min}' > temp1.txt

        echo -e "Results\nAvg\nMax\nMin" > temp0.txt
        paste temp0.txt temp1.txt > temp-total.txt
        column -t temp-total.txt
        rm temp*.txt

        #Plot u2u
        cat $RX_FILENAME | \
                         awk '{ print $1 }' > $SHORTNAME-traffic.txt
}

calc_return_u2u(){
        # Output format (return):
        # a2bLatency, seqA, sdata->id, txPubA, RXhwTS, rxSubB, processingLatency,
        # b2aLatency, seqB, sdata->id, txPubB, RXhwTS, rxSubA, returnLatency

        local RX_FILENAME=$1 #*-rxtstamps.txt
        SHORTNAME=$(echo $RX_FILENAME | awk -F"-" '{print $1}')

        #U2U stats
        cat $RX_FILENAME | \
                awk '{ print $14 "\t" $9}' | \
                awk '{ if($2 > 0){
                        if(min==""){min=max=$1};
                        if($1>max) {max=$1};
                        if($1<min) {min=$1};
                        total+=$1; count+=1
                        }
                      } END {print "U2U\n"total/count,"\n"max,"\n"min}' > temp1.txt

        echo -e "Results\nAvg\nMax\nMin" > temp0.txt
        paste temp0.txt temp1.txt > temp-total.txt
        column -t temp-total.txt
        rm temp*.txt

        #Plot u2u
        cat $RX_FILENAME | \
                         awk '{ print $14 }' > $SHORTNAME-traffic.txt
}


calc_rx_duploss(){
        # Output format (1 way):
        # latency, rx_sequence, sdata->id, txTime, RXhwTS, rxTime

        local XDP_TX_FILENAME=$1 #*-txtstamps.txt
        local JSON_FILE=$2 #Json configuration file

        # Packet count from json file
        PACKET_COUNT=$(grep -s packet_count $JSON_FILE | awk '{print $2}' | sed 's/,//')
        echo $PACKET_COUNT >> temp1.txt

        # Total packets received
        PACKET_RX=$(cat $XDP_TX_FILENAME \
                | awk '{print $2}' \
                | grep -x -E '[0-9]+' \
                | wc -l)
        echo $PACKET_RX >> temp1.txt

        # Total duplicate
        PACKET_DUPL=$(cat $XDP_TX_FILENAME \
                | awk '{print $2}' \
                | uniq -D \
                | wc -l)
        echo $PACKET_DUPL >> temp1.txt

        # Total missing: Same as: packet count - total_packet - total_duplicate
        PACKET_LOSS=$((PACKET_COUNT-PACKET_RX-PACKET_DUPL))
        echo $PACKET_LOSS >> temp1.txt

        echo -e "TotalExpected\nTotalReceived\nDuplicates\nLosses" > temp0.txt
        paste temp0.txt temp1.txt > temp-total.txt
        column -t temp-total.txt
        rm temp*.txt
}

calc_return_duploss(){
        # Output format (return):
        # a2bLatency, seqA, sdata->id, txPubA, RXhwTS, rxSubB, processingLatency,
        # b2aLatency, seqB, sdata->id, txPubB, RXhwTS, rxSubA, returnLatency

        local XDP_TX_FILENAME=$1 #*-txtstamps.txt
        local JSON_FILE=$2 #Json configuration file

        # Packet count from json file
        PACKET_COUNT=$(grep -s packet_count $JSON_FILE | awk '{print $2}' | sed 's/,//')
        echo $PACKET_COUNT >> temp1.txt

        # Total packets received
        PACKET_RX=$(cat $XDP_TX_FILENAME \
                | awk '{print $9}' \
                | grep -x -E '[0-9]+' \
                | wc -l)
        echo $PACKET_RX >> temp1.txt

        # Total duplicate
        PACKET_DUPL=$(cat $XDP_TX_FILENAME \
                | awk '{print $9}' \
                | uniq -D \
                | wc -l)
        echo $PACKET_DUPL >> temp1.txt

        # Total missing: Same as: packet count - total_packet - total_duplicate
        PACKET_LOSS=$((PACKET_COUNT-PACKET_RX-PACKET_DUPL))
        echo $PACKET_LOSS >> temp1.txt

        echo -e "TotalExpected\nTotalReceived\nDuplicates\nLosses" > temp0.txt
        paste temp0.txt temp1.txt > temp-total.txt
        column -t temp-total.txt
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

        rm plot_pic.png -f

        CONFIG=$1
        NUMPKTS=$2
        SIZE=$3
        INTERVAL=$4

        case "$CONFIG" in

        opcua-pkt0b | opcua-pkt1b | opcua-pkt2a | opcua-pkt2b | opcua-pkt3a | opcua-pkt3b)
                cp afpkt-rxtstamps.txt results-$ID/afpkt-$CONFIG-rxtstamps-$IDD.txt
        ;;
        opcua-xdp0b | opcua-xdp1b | opcua-xdp2a | opcua-xdp2b | opcua-xdp3a | opcua-xdp3b)
                cp afxdp-rxtstamps.txt results-$ID/afxdp-$CONFIG-rxtstamps-$IDD.txt
        ;;
        *)
                echo "Error: save_results_files() invalid config: $CONFIG"
                exit
        ;;
        esac
}
