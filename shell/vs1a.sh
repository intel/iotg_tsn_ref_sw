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

# Get this script's dir because cfg file is stored together with this script
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source $DIR/helpers.sh
source $DIR/$PLAT/$(basename -s ".sh" $0).config

if [ -z $1 ]; then
        echo "Specify interface"; exit
elif [[ -z $INTERVAL || -z $XDP_INTERVAL ||
       -z $EARLY_OFFSET || -z $XDP_EARLY_OFFSET ||
       -z $TXTIME_OFFSET || -z $NUMPKTS || -z $SIZE ||
       -z $TX_PKT_Q || -z $TX_XDP_Q || -z $XDP_MODE ||
       -z $TEMP_DIR ]]; then
        echo "Source config file first"; exit
fi

pkill gnuplot
pkill txrx-tsn
pkill iperf3

# Remove logging files and its links
rm -f $TEMP_DIR/afpkt-txtstamps.txt
rm -f $TEMP_DIR/afxdp-txtstamps.txt
rm -f ./afpkt-txtstamps.txt
rm -f ./afxdp-txtstamps.txt
sync

# Improve performance/consistency by logging to tmpfs (system memory)
check_and_mount_tmpfs $TEMP_DIR
ln -sfv $TEMP_DIR/afpkt-txtstamps.txt .
ln -sfv $TEMP_DIR/afxdp-txtstamps.txt .

SLEEP_SEC=$(((($NUMPKTS * $INTERVAL) / $SEC_IN_NSEC) + 10))
XDP_SLEEP_SEC=$(((($NUMPKTS * $XDP_INTERVAL) / $SEC_IN_NSEC) + 50))
KERNEL_VER=$(uname -r | cut -d'.' -f1-2)

if [ "$AFP_PACKET_TEST" = "y" ]; then
        echo "PHASE 1: AF_PACKET transmit ($SLEEP_SEC seconds)"

        if [ "$RUN_IPERF3_AFP" = "y" ]; then
                run_iperf3_bg_client
        fi
        sleep 5

        echo "CMD: ./txrx-tsn -i $IFACE -PtTq $TX_PKT_Q -n $NUMPKTS -l $SIZE -y $INTERVAL -e $EARLY_OFFSET -o $TXTIME_OFFSET"
        ./txrx-tsn -i $IFACE -PtTq $TX_PKT_Q -n $NUMPKTS -l $SIZE -y $INTERVAL \
                        -e $EARLY_OFFSET -o $TXTIME_OFFSET > afpkt-txtstamps.txt &
        TXRX_PID=$!

        if ! ps -p $TXRX_PID > /dev/null; then
                echo -e "\ntxrx-tsn exited prematurely. vs1a.sh script will be stopped."
                kill -9 $( pgrep -x iperf3 ) > /dev/null
                exit 1
        fi

        # Assign to CPU2
        taskset -p $TXRX_TSN_AFFINITY $TXRX_PID
        chrt --fifo -p 90 $TXRX_PID

        sleep $SLEEP_SEC
        pkill iperf3
        pkill txrx-tsn
else
        echo "PHASE 1: AF_PACKET is not configured to run."
fi

sleep 20
# If AF_XDP is not available/not supported for the platform, we will exit.
if [[ "$XDP_MODE" == "NA" ]]; then
    echo "PHASE 2: Skipped. Currently $PLAT does not support AF_XDP."
    echo "Done!"
    exit 0
else
    sleep 20

    echo "PHASE 2: AF_XDP transmit $XDP_SLEEP_SEC seconds)"

    if [ "$RUN_IPERF3_XDP" = "y" ]; then
        run_iperf3_bg_client
    fi

    # This is targeting for kernel 5.* only
    # For kernel 5.*, we will run the txrx-tsn before running the interface and clock configuration.
    # For kernel 6.* and above, we will run the txrx-tsn after the interface and clock configuration.
    if [[ $KERNEL_VER == 5.* ]]; then
        echo "CMD: ./txrx-tsn -X -$XDP_MODE -ti $IFACE -q $TX_XDP_Q -n $NUMPKTS -l $SIZE -y $XDP_INTERVAL -e $XDP_EARLY_OFFSET -o $TXTIME_OFFSET"
        ./txrx-tsn -X -$XDP_MODE -ti $IFACE -q $TX_XDP_Q -n $NUMPKTS -l $SIZE -y $XDP_INTERVAL \
        -e $XDP_EARLY_OFFSET -o $TXTIME_OFFSET > afxdp-txtstamps.txt &

        TXRX_PID=$!

        if ! ps -p $TXRX_PID > /dev/null; then
            echo -e "\ntxrx-tsn exited prematurely. vs1a.sh script will be stopped."
            exit 1
        fi

        # Assign to CPU2
        taskset -p $TXRX_TSN_AFFINITY $TXRX_PID
        chrt --fifo -p 90 $TXRX_PID

        sleep 5
    fi

    # This is targeting for stmmac and kernel others than 5.4
    if [[ $PLAT != i225* && $KERNEL_VER != 5.4 ]]; then
        init_interface  $IFACE
        setup_taprio $IFACE
        setup_etf $IFACE
        # Disable the coalesce
        echo "[Kernel_${KERNEL_VER}_XDP] Disable coalescence."
        ethtool --per-queue $IFACE queue_mask 0x0F --coalesce rx-usecs 21 rx-frames 1 tx-usecs 1 tx-frames 1
        sleep 2
        setup_vlanrx_xdp $IFACE
        $DIR/clock-setup.sh $IFACE
        sleep 30
    # This is targeting for i225/i226 and kernel others than 5.4
    elif [[ $PLAT == i225* && $KERNEL_VER != 5.4 ]]; then
        # Disable the coalesce
        echo "[Kernel_${KERNEL_VER}_XDP_i225] Disable coalescence."
        ethtool -C $IFACE rx-usecs 0
        sleep 50
    else
        sleep 40
    fi

    # This is targeting for kernel others than 5.*
    if [[ $KERNEL_VER != 5.* ]]; then
        echo "CMD: ./txrx-tsn -X -$XDP_MODE -ti $IFACE -q $TX_XDP_Q -n $NUMPKTS -l $SIZE -y $XDP_INTERVAL -e $XDP_EARLY_OFFSET -o $TXTIME_OFFSET"
        ./txrx-tsn -X -$XDP_MODE -ti $IFACE -q $TX_XDP_Q -n $NUMPKTS -l $SIZE -y $XDP_INTERVAL \
        -e $XDP_EARLY_OFFSET -o $TXTIME_OFFSET > afxdp-txtstamps.txt &

        TXRX_PID=$!

        if ! ps -p $TXRX_PID > /dev/null; then
            echo -e "\ntxrx-tsn exited prematurely. vs1a.sh script will be stopped."
            exit 1
        fi

        # Assign to CPU2
        taskset -p $TXRX_TSN_AFFINITY $TXRX_PID
        chrt --fifo -p 90 $TXRX_PID

        sleep 5
    fi

    sleep $XDP_SLEEP_SEC
    pkill iperf3
    pkill txrx-tsn

    # This is targeting for i225/i226 and kernel 5.4 only
    if [[ $PLAT == i225* && $KERNEL_VER == 5.4 ]]; then
        # To ensure the AF_XDP socket tear down is complete in i225, interface is reset.
        echo "Re run vs1a setup for af_xdp operation"
        setup_link_down_up $IFACE
        sleep 2
        sh $DIR/setup-vs1a.sh $IFACE
    fi

        echo "Done!"
fi

exit 0
