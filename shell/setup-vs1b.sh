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
source $DIR/$PLAT/$CONFIG.config

if [ -z $1 ]; then
        echo "Specify interface"; exit
else
        IFACE=$1
fi

#Each func/script has their own basic input validation - apart from $IFACE

init_interface  $IFACE

$DIR/clock-setup.sh $IFACE
sleep 50 #Give some time for clock daemons to start.

setup_mqprio $IFACE
sleep 10

sleep 10 # just to make it same delay as vs1a.sh

if [[ $PLAT == i225* ]]; then
        RULES63=$(ethtool -n $IFACE | grep "Filter: 63")
        if [[ ! -z $RULES63 ]]; then
                echo "Deleting filter rule 63"
                ethtool -N $IFACE delete 63
        fi
        RULES62=$(ethtool -n $IFACE | grep "Filter: 62")
        if [[ ! -z $RULES62 ]]; then
                echo "Deleting filter rule 62"
                ethtool -N $IFACE delete 62
        fi
        RULES61=$(ethtool -n $IFACE | grep "Filter: 61")
        if [[ ! -z $RULES61 ]]; then
                echo "Deleting filter rule 61"
                ethtool -N $IFACE delete 61
        fi
        RULES60=$(ethtool -n $IFACE | grep "Filter: 60")
        if [[ ! -z $RULES60 ]]; then
                echo "Deleting filter rule 60"
                ethtool -N $IFACE delete 60
        fi

        # Use flow-type to push ptp packet to $PTP_RX_Q
        echo "Adding flow-type for ptp packet to q-$PTP_RX_Q"
        echo "ethtool -N $IFACE flow-type ether proto 0x88f7 queue $PTP_RX_Q"
        ethtool -N $IFACE flow-type ether proto 0x88f7 queue $PTP_RX_Q

        # Use flow-type to push txrx-tsn packet VLAN PRIORITY 3 to $RX_PKT_Q
        echo "Adding flow-type for txrx-tsn packet vlan priority $VLAN_PRIOR_PKT to q-$RX_PKT_Q"
        echo "ethtool -N $IFACE flow-type ether vlan $VLAN_PRIOR_PKT vlan-mask 0x1FFF action $RX_PKT_Q"
        ethtool -N $IFACE flow-type ether vlan $VLAN_PRIOR_PKT vlan-mask 0x1FFF action $RX_PKT_Q

        # Use flow-type to push txrx-tsn packet VLAN PRIORITY 2 to $RX_XDP_Q
        echo "Adding flow-type for txrx-tsn packet priority $VLAN_PRIOR_AF_XDP to q-$RX_XDP_Q"
        echo "ethtool -N $IFACE flow-type ether vlan $VLAN_PRIOR_AF_XDP vlan-mask 0x1FFF action $RX_XDP_Q"
        ethtool -N $IFACE flow-type ether vlan $VLAN_PRIOR_AF_XDP vlan-mask 0x1FFF action $RX_XDP_Q

        # Use flow-type to push iperf3 packet to $IPERF_Q
        echo "Adding flow-type for iperf3 packet to q-$IPERF_Q"
        echo "ethtool -N $IFACE flow-type ether proto 0x0800 queue $IPERF_Q"
        ethtool -N $IFACE flow-type ether proto 0x0800 queue $IPERF_Q

else
        setup_vlanrx $IFACE
fi

echo "Wait 30 sec for PTP to sync"
sleep 30 #Give some time for gPTP clock to sync

exit 0
