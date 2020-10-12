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
SEC_IN_NSEC=1000000000

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
        ip link set $IFACE.vlan type vlan egress-qos-map 6:6

        # Flush neighbours, just in case
        ip neigh flush all dev $IFACE
        ip neigh flush all dev $IFACE.vlan

        # Turn off VLAN Stripping
        ethtool -K $IFACE rxvlan off

        # Disable EEE
        ethtool --set-eee $IFACE eee off 2&> /dev/null

        if [ $TXQ_COUNT -eq 8 ]; then
                set_irq_smp_affinity $IFACE $DIR/irq_affinity_4c_8tx_8rx.map
        else
                set_irq_smp_affinity $IFACE $DIR/irq_affinity_4c_4tx_4rx.map
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
        ip link set $IFACE.vlan type vlan egress-qos-map 6:6

        # Flush neighbours, just in case
        ip neigh flush all dev $IFACE
        ip neigh flush all dev $IFACE.vlan

        # Turn off VLAN Stripping
        ethtool -K $IFACE rxvlan off

        # Disable EEE
        ethtool --set-eee $IFACE eee off 2&> /dev/null

        if [ $TXQ_COUNT -eq 8 ]; then
                set_irq_smp_affinity $IFACE $DIR/irq_affinity_4c_8tx_8rx.map
        else
                set_irq_smp_affinity $IFACE $DIR/irq_affinity_4c_4tx_4rx.map
        fi
}

get_XDPTXQ_NUM(){
        IFACE=$1

        TXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==4{ print $2}')

        if [ $TXQ_COUNT -eq 8 ]; then
                XDPTXQ_NUM=2 #EHL-HWTXq6
        elif [ $TXQ_COUNT -eq 4 ]; then
                XDPTXQ_NUM=1 #TGL-HWTXq3
        else
                echo "get_XDPTXQ_NUM()- invalid TX queue count"
        fi
}

get_TXQ_NUM(){
        IFACE=$1

        TXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==4{ print $2}')

        if [ $TXQ_COUNT -eq 8 ]; then
                TXQ_NUM=6 #EHL-HWTXq6, XDPTXq2
        elif [ $TXQ_COUNT -eq 4 ]; then
                TXQ_NUM=3 #TGL-HWTXq3, XDPTXq1
        else
                echo "get_TXQ_NUM()- invalid TX queue count"
        fi
}

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
                      } END {print "U2U\n"total/count,"\n"max,"\n"min}' > temp1.txt

        echo -e "Results\nAvg\nMax\nMin" > temp0.txt
        paste temp0.txt temp1.txt > temp-total.txt
        column -t temp-total.txt
        rm temp*.txt

        #Plot u2u
        cat $RX_FILENAME | \
                         awk '{ print $1 }' > $SHORTNAME-traffic.txt
}

calc_rx_duploss(){
        local XDP_RX_FILENAME=$1 #*-txtstamps.txt

        # Total packets
        cat $XDP_RX_FILENAME \
                | awk '{print $2}' \
                | grep -x -E '[0-9]+' \
                | wc -l >> temp1.txt

        # Total duplicate
        cat $XDP_RX_FILENAME \
                | awk '{print $2}' \
                | uniq -D \
                | wc -l >> temp1.txt

        # Total missing: Same as: total_packet - total_unique (uniq -c)
        cat $XDP_RX_FILENAME \
                | awk '{print $2}' \
                | grep -x -E '[0-9]+' \
                | awk '{for(i=p+1; i<$1; i++) print i} {p=$1}' \
                | wc -l >> temp1.txt

        echo -e "Total\nDuplicates\nLosses" > temp0.txt
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

        rm -f plot_pic.png #remove existing plot files.

        CONFIG=$1
        NUMPKTS=$2
        SIZE=$3
        INTERVAL=$4

        case "$CONFIG" in

        vs1b)
                cp afxdp-rxtstamps.txt results-$ID/afxdp-$CONFIG-$NUMPKTS-$SIZE-$XDP_INTERVAL-rxtstamps-$IDD.txt
                cp afpkt-rxtstamps.txt results-$ID/afpkt-$CONFIG-$NUMPKTS-$SIZE-$INTERVAL-rxtstamps-$IDD.txt
        ;;

        *)
                echo "Error: save_results_files() invalid config: $CONFIG"
                exit
        ;;
        esac
}
