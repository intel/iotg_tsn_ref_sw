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

pkill gnuplot
pkill txrx-tsn
pkill iperf3

if [ $# -eq 3 ]; then
        IFACE=$1
        NUMPKTS=$2
        SIZE=$3
else
        echo "Note: Using default PKT3a params"
        IFACE=$1
        NUMPKTS=1000000
        SIZE=64
fi

TXTIME_OFFSET=20000

INTERVAL=1000000
EARLY_OFFSET=700000
XDP_INTERVAL=200000
XDP_EARLY_OFFSET=100000

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source $DIR/helpers.sh

SLEEP_SEC=$(((($NUMPKTS * $INTERVAL) / $SEC_IN_NSEC) + 10))
XDP_SLEEP_SEC=$(((($NUMPKTS * $XDP_INTERVAL) / $SEC_IN_NSEC) + 10))

# Improve performance/consistency by logging to tmpfs (system memory)
ln -sfv /tmp/afpkt-rxtstamps.txt .
ln -sfv /tmp/afxdp-rxtstamps.txt .

echo 0 > afpkt-rxtstamps.txt
echo 0 > afxdp-rxtstamps.txt
echo 0 > afpkt-traffic.txt
echo 0 > afxdp-traffic.txt

echo "PHASE 1: AF_PACKET receive ($SLEEP_SEC seconds)"
$DIR/iperf3-bg-server.sh
sleep 5

get_TXQ_NUM $IFACE

./txrx-tsn -Pri $IFACE -q $TXQ_NUM > afpkt-rxtstamps.txt &
TXRX_PID=$!

if ! ps -p $TXRX_PID > /dev/null; then
	echo -e "\ntxrx-tsn exited prematurely. vs1b.sh script will be stopped."
	kill -9 $( pgrep -x iperf3 ) > /dev/null
	exit 1
fi

# Assign to CPU2
taskset -p 4 $TXRX_PID
sleep $SLEEP_SEC
pkill txrx-tsn
pkill iperf3

echo "PHASE 2: AF_XDP receive ($XDP_SLEEP_SEC seconds)"
$DIR/iperf3-bg-server.sh
sleep 5

get_XDPTXQ_NUM $IFACE

./txrx-tsn -Xzri $IFACE -q $XDPTXQ_NUM > afxdp-rxtstamps.txt &
TXRX_PID=$!

if ! ps -p $TXRX_PID > /dev/null; then
	echo -e "\ntxrx-tsn exited prematurely. vs1b.sh script will be stopped."
	kill -9 $( pgrep -x iperf3 ) > /dev/null
	exit 1
fi

# Assign to CPU2
taskset -p 4 $TXRX_PID
sleep $XDP_SLEEP_SEC
pkill iperf3
pkill txrx-tsn

echo "PHASE 3: Calculating.."
pkill gnuplot

stop_if_empty "afpkt-rxtstamps.txt"
calc_rx_u2u "afpkt-rxtstamps.txt"
calc_rx_duploss "afpkt-rxtstamps.txt"

stop_if_empty "afxdp-rxtstamps.txt"
calc_rx_u2u "afxdp-rxtstamps.txt"
calc_rx_duploss "afxdp-rxtstamps.txt"

save_result_files $(basename $0 .sh) $NUMPKTS $SIZE $INTERVAL

gnuplot -e "FILENAME='afpkt-traffic.txt';FILENAME2='afxdp-traffic.txt';" $DIR/latency_dual.gnu -p 2> /dev/null &

while [[ ! -s plot_pic.png ]]; do sleep 5; done
cp plot_pic.png results-$ID/plot-$(basename $0 .sh)-$NUMPKTS-$SIZE-$INTERVAL-$XDP_INTERVAL-$IDD.png
