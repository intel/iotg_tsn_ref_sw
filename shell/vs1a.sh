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
       -z $TX_PKT_Q || -z $TX_XDP_Q || -z $XDP_MODE ]]; then
        echo "Source config file first"; exit
fi

pkill gnuplot
pkill txrx-tsn
pkill iperf3

# Improve performance/consistency by logging to tmpfs (system memory)
ln -sfv /tmp/afpkt-txtstamps.txt .
ln -sfv /tmp/afxdp-txtstamps.txt .

SLEEP_SEC=$(((($NUMPKTS * $INTERVAL) / $SEC_IN_NSEC) + 10))
XDP_SLEEP_SEC=$(((($NUMPKTS * $XDP_INTERVAL) / $SEC_IN_NSEC) + 10))

echo "PHASE 1: AF_PACKET transmit ($SLEEP_SEC seconds)"
run_iperf3_bg_client
sleep 5

./txrx-tsn -i $IFACE -PtTq $TX_PKT_Q -n $NUMPKTS -l $SIZE -y $INTERVAL \
                -e $EARLY_OFFSET -o $TXTIME_OFFSET > afpkt-txtstamps.txt &
TXRX_PID=$!

if ! ps -p $TXRX_PID > /dev/null; then
	echo -e "\ntxrx-tsn exited prematurely. vs1a.sh script will be stopped."
	kill -9 $( pgrep -x iperf3 ) > /dev/null
	exit 1
fi

# Assign to CPU2
taskset -p 4 $TXRX_PID
chrt --fifo -p 90 $TXRX_PID

sleep $SLEEP_SEC
pkill iperf3
pkill txrx-tsn

echo "PHASE 2: AF_XDP transmit ($XDP_SLEEP_SEC seconds)"
run_iperf3_bg_client
sleep 5

./txrx-tsn -X -$XDP_MODE -ti $IFACE -q $TX_XDP_Q -n $NUMPKTS -l $SIZE -y $XDP_INTERVAL \
                -e $XDP_EARLY_OFFSET -o $TXTIME_OFFSET > afxdp-txtstamps.txt &
TXRX_PID=$!

if ! ps -p $TXRX_PID > /dev/null; then
	echo -e "\ntxrx-tsn exited prematurely. vs1a.sh script will be stopped."
	kill -9 $( pgrep -x iperf3 ) > /dev/null
	exit 1
fi

# Assign to CPU2
taskset -p 4 $TXRX_PID
chrt --fifo -p 90 $TXRX_PID

sleep $XDP_SLEEP_SEC
pkill iperf3
pkill txrx-tsn

echo Done!
exit
