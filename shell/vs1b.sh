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

# Get this script's dir because cfg file is stored together with this script
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source $DIR/helpers.sh
source $DIR/$PLAT/$(basename -s ".sh" $0).config

if [ -z $1 ]; then
        echo "Specify interface"; exit
elif [[ -z $INTERVAL || -z $XDP_INTERVAL ||
       -z $EARLY_OFFSET || -z $XDP_EARLY_OFFSET ||
       -z $TXTIME_OFFSET || -z $NUMPKTS || -z $SIZE ||
       -z $RX_PKT_Q || -z $RX_XDP_Q || -z $XDP_MODE ]]; then
        echo "Source config file first"; exit
fi

pkill gnuplot
pkill txrx-tsn
pkill iperf3

# Improve performance/consistency by logging to tmpfs (system memory)
ln -sfv /tmp/afpkt-rxtstamps.txt .
ln -sfv /tmp/afxdp-rxtstamps.txt .

echo 0 > afpkt-rxtstamps.txt
echo 0 > afxdp-rxtstamps.txt
echo 0 > afpkt-traffic.txt
echo 0 > afxdp-traffic.txt

SLEEP_SEC=$(((($NUMPKTS * $INTERVAL) / $SEC_IN_NSEC) + 10))
XDP_SLEEP_SEC=$(((($NUMPKTS * $XDP_INTERVAL) / $SEC_IN_NSEC) + 10))

echo "PHASE 1: AF_PACKET receive ($SLEEP_SEC seconds)"
run_iperf3_bg_server
sleep 5

./txrx-tsn -Pri $IFACE -q $RX_PKT_Q > afpkt-rxtstamps.txt &
TXRX_PID=$!

if ! ps -p $TXRX_PID > /dev/null; then
	echo -e "\ntxrx-tsn exited prematurely. vs1b.sh script will be stopped."
	kill -9 $( pgrep -x iperf3 ) > /dev/null
	exit 1
fi

# Assign to CPU3
taskset -p 8 $TXRX_PID
sleep $SLEEP_SEC
pkill txrx-tsn
pkill iperf3

if [ "$XDP_MODE" = "NA" ]; then
    echo "PHASE 2: Skipped. Currently $PLAT does not support AF_XDP."
else
    sleep 20

    echo "PHASE 2: AF_XDP receive ($XDP_SLEEP_SEC seconds)"
    run_iperf3_bg_server
    sleep 5

    ./txrx-tsn -X -$XDP_MODE -ri $IFACE -q $RX_XDP_Q > afxdp-rxtstamps.txt &
    TXRX_PID=$!

    if ! ps -p $TXRX_PID > /dev/null; then
        echo -e "\ntxrx-tsn exited prematurely. vs1b.sh script will be stopped."
        kill -9 $( pgrep -x iperf3 ) > /dev/null
        exit 1
    fi

    # Assign to CPU3
    taskset -p 8 $TXRX_PID
    sleep $XDP_SLEEP_SEC
    pkill iperf3
    pkill txrx-tsn
fi

echo "PHASE 3: Calculating.."
pkill gnuplot

stop_if_empty "afpkt-rxtstamps.txt"
calc_rx_u2u "afpkt-rxtstamps.txt"
calc_rx_duploss "afpkt-rxtstamps.txt"

if [ "$XDP_MODE" != "NA" ]; then
    stop_if_empty "afxdp-rxtstamps.txt"
    calc_rx_u2u "afxdp-rxtstamps.txt"
    calc_rx_duploss "afxdp-rxtstamps.txt"
fi

save_result_files $(basename $0 .sh) $NUMPKTS $SIZE $INTERVAL $XDP_MODE $PLAT

if [ "$XDP_MODE" = "NA" ]; then
    gnuplot -e "FILENAME='afpkt-traffic.txt';YMAX=2000000; PLOT_TITLE='$PLAT:AF_Packet only'" $DIR/../common/latency_single.gnu -p 2> /dev/null &
else
    gnuplot -e "FILENAME='afpkt-traffic.txt';FILENAME2='afxdp-traffic.txt';" $DIR/../common/latency_dual.gnu -p 2> /dev/null &
fi

while [[ ! -s plot_pic.png ]]; do sleep 5; done
cp plot_pic.png results-$ID/$PLAT-plot-$(basename $0 .sh)-$NUMPKTS-$SIZE-$INTERVAL-$XDP_INTERVAL-$IDD.png

exit 0
