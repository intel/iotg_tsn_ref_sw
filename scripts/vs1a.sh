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

# Get this script's dir because cfg file is stored together with this script
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source $DIR/helpers.sh

SLEEP_SEC=$(((($NUMPKTS * $INTERVAL) / $SEC_IN_NSEC) + 10))
XDP_SLEEP_SEC=$(((($NUMPKTS * $XDP_INTERVAL) / $SEC_IN_NSEC) + 10))

echo "PHASE 1: AF_PACKET transmit ($SLEEP_SEC seconds)"
$DIR/iperf3-bg-client.sh
sleep 5

get_TXQ_NUM $IFACE

./txrx-tsn -i $IFACE -PtTq $TXQ_NUM -n $NUMPKTS -l $SIZE -y $INTERVAL \
                -e $EARLY_OFFSET -o $TXTIME_OFFSET > /dev/shm/afpkt-txtstamps.txt &
TXRX_PID=$!
taskset -p 4 $TXRX_PID
chrt --fifo -p 90 $TXRX_PID
ps -o psr,pri,pid,cmd $TXRX_PID

sleep $SLEEP_SEC
pkill iperf3
pkill txrx-tsn

cp /dev/shm/afpkt-txtstamps.txt . &

echo "PHASE 2: AF_XDP transmit ($XDP_SLEEP_SEC seconds)"
$DIR/iperf3-bg-client.sh
sleep 5

get_XDPTXQ_NUM $IFACE

./txrx-tsn -XztTi $IFACE -q $XDPTXQ_NUM -n $NUMPKTS -l $SIZE -y $XDP_INTERVAL \
                -e $XDP_EARLY_OFFSET -o $TXTIME_OFFSET > /dev/shm/afxdp-txtstamps.txt &
TXRX_PID=$!
taskset -p 4 $TXRX_PID
chrt --fifo -p 90 $TXRX_PID
ps -o psr,pri,pid,cmd $TXRX_PID

sleep $XDP_SLEEP_SEC
pkill iperf3
pkill txrx-tsn

cp /dev/shm/afxdp-txtstamps.txt .

echo Done!
exit
