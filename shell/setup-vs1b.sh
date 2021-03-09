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
source $DIR/$PLAT/$CONFIG.config

if [ -z $1 ]; then
        echo "Specify interface"; exit
else
        IFACE=$1
fi

#Each func/script has their own basic input validation - apart from $IFACE

init_interface  $IFACE

$DIR/clock-setup.sh $IFACE
sleep 30 #Give some time for clock daemons to start.

setup_mqprio $IFACE
sleep 10

if [[ $PLAT == i225* ]]; then
        RULES31=$(ethtool -n enp169s0 | grep "Filter: 31")
        if [[ ! -z $RULES31 ]]; then
                echo "Deleting filter rule 31"
                ethtool -N enp169s0 delete 31
        fi
        RULES30=$(ethtool -n enp169s0 | grep "Filter: 30")
        if [[ ! -z $RULES30 ]]; then
                echo "Deleting filter rule 30"
                ethtool -N enp169s0 delete 30
        fi
        RULES29=$(ethtool -n enp169s0 | grep "Filter: 29")
        if [[ ! -z $RULES30 ]]; then
                echo "Deleting filter rule 29"
                ethtool -N enp169s0 delete 29
        fi

        # Use flow-type to push ptp packet to $PTP_RX_Q
        ethtool -N $IFACE flow-type ether proto 0x88f7 queue $PTP_RX_Q
        echo "Adding flow-type for ptp packet to q-$PTP_RX_Q"

        # Use flow-type to push txrx-tsn packet packet to $RX_PKT_Q
        ethtool -N $IFACE flow-type ether vlan 24576 vlan-mask 0x1FFF action $RX_PKT_Q
        echo "Adding flow-type for txrx-tsn packet to q-$RX_PKT_Q"

        # Use flow-type to push iperf3 packet to 0
        ethtool -N $IFACE flow-type ether proto 0x0800 queue 0
        echo "Adding flow-type for iperf3 packet to q-0"

else
        setup_vlanrx $IFACE
fi

sleep 10

exit 0
