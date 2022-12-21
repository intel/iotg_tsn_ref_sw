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

# Get directory of current script
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source $DIR/$PLAT/$CONFIG.config

if [ -z $1 ]; then
    echo "Please specify interface: ./clock-setup.sh eth0"
    exit 1
fi

IFACE=$1
pkill ptp4l
pkill phc2sys

# Defaults when executing clock-setup.sh directly
# Use q0, gPTP.cfg and no vlan
if [ -z $PTP_TX_Q ]; then
    PTP_TX_Q=0;
    echo "Using default $PTP_TX_Q";
fi

echo "Running PTP4L & PHC2SYS"

if [ -z $PTP_PHY_HW ]; then
    GPTP_FILE="gPTP.cfg"
    echo "Using default gPTP.cfg";
else
    GPTP_FILE="gPTP_$PTP_PHY_HW.cfg"
    echo "Using $GPTP_FILE";
fi

taskset -c 1 ptp4l -P2Hi $IFACE$PTP_IFACE_APPEND -f $DIR/../common/$GPTP_FILE \
        --step_threshold=1 --socket_priority=$PTP_TX_Q -m &> /var/log/ptp4l.log &

sleep 2 # Required

pmc -u -b 0 -t 1 "SET GRANDMASTER_SETTINGS_NP clockClass 248
        clockAccuracy 0xfe offsetScaledLogVariance 0xffff currentUtcOffset 37
        leap61 0 leap59 0 currentUtcOffsetValid 1 ptpTimescale 1 timeTraceable
        1 frequencyTraceable 0 timeSource 0xa0" &> /var/log/pmc.log

sleep 3

if [[ $PLAT == i225* ]]; then
    taskset -c 1 phc2sys -s $IFACE -c CLOCK_REALTIME --step_threshold=1 \
        --transportSpecific=1 -O 0 --first_step_threshold=0.0 \
        -w -ml 7 &> /var/log/phc2sys.log &
else
    taskset -c 1 phc2sys -s $IFACE -c CLOCK_REALTIME --step_threshold=1 \
            --transportSpecific=1 -O 0 -w -ml 7 &> /var/log/phc2sys.log &
fi
