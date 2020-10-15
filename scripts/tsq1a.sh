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

if [ $# -eq 0 ]; then
	echo "How to run this : $0 <interface> < secs ( opt : how long to run the test , def : 50 sec ) >"
	exit 1
fi

IFACE=$1

if [ $# -eq 2 ]; then
	TEST_PERIOD=$2
else
	TEST_PERIOD=60
fi

IPADDR="169.254.1.11"

if pgrep -x tsq > /dev/null; then
	kill -9 $( pgrep -x tsq ) > /dev/null
	echo -e "Previous TSQ is still running. Attempting to kill it."
fi
pkill gnuplot

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

./tsq -L -i $IPADDR -p 7777 -v &
TSQ_LISTENER_PID=$!

# Check if tsq listener is alive, otherwise exit.
if ! ps -p $TSQ_LISTENER_PID > /dev/null; then
	echo -e "\nTSQ Listener has exited prematurely. Script will stop now."
	exit 1
fi

sleep 5

CLK=`ethtool -T $IFACE | grep -Po "(?<=PTP Hardware Clock: )[\d+]"`

./tsq -T -i $IPADDR -p 7777 -d /dev/ptp$CLK -v -u 1111  &
TSQ_TALKER_PID=$!

# Check if tsq talker is alive, otherwise exit
if ! ps -p $TSQ_TALKER_PID > /dev/null; then
	echo -e "\nTSQ Talker has exited prematurely. Script will stop now."
	# Kill tsq listener before exiting.
	kill -9 $( pgrep -x tsq ) > /dev/null
	exit 1
fi

$DIR/enable_extts.sh $IFACE

while [ ! -f tsq-listener-data.txt ]
do
	sleep 1
	echo "Waiting for data file"
done

if [[ $DISPLAY ]]; then
	echo "Starting plotting"
	gnuplot -e "filename='tsq-listener-data.txt'" $DIR/liveplot.gnu &
fi

sleep $TEST_PERIOD
kill -9 $( pgrep -x tsq ) > /dev/null

echo "Plot data file : tsq-listener-data.txt is now ready."

exit 0

