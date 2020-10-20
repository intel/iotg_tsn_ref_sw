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

IFACE=$1

if [ $# -ne 1 ];then
        echo "Please enter interface : ./setup_taprio.sh eth0"
        exit 1
fi

echo "Running TC-TAPRIO command"

# tc qdisc del dev $IFACE parent root 2> /dev/null

TXQ_COUNT=$(ethtool -l $IFACE | awk 'NR==4{ print $2}')
BASE=$(expr $(date +%s) + 5)000000000

# To use replace, we need a base for the first time.
# This command is does nothing if used when there's an existing qdisc.
tc qdisc add dev $IFACE root mqprio \
        num_tc 4 map 0 1 2 3 3 3 3 3 3 3 3 3 3 3 3 0 \
        queues 1@0 1@1 1@2 1@3 hw 0 2&> /dev/null

if [ $TXQ_COUNT -eq 8 ]; then
        tc qdisc replace dev $IFACE parent root handle 100 taprio \
                num_tc 8 map 0 1 2 3 4 5 6 7 0 0 0 0 0 0 0 0 \
                queues 1@0 1@1 1@2 1@3 1@4 1@5 1@6 1@7 \
                base-time $BASE \
                sched-entry S 43 500000 \
                sched-entry S 42 500000 \
                flags 0x2
elif [ $TXQ_COUNT -eq 4 ]; then
        tc qdisc replace dev $IFACE parent root handle 100 taprio \
                num_tc 4 map 0 1 2 3 0 0 0 0 0 0 0 0 0 0 0 0 \
                queues 1@0 1@1 1@2 1@3 \
                base-time $BASE \
                sched-entry S 0F 500000 \
                sched-entry S 0E 500000 \
                flags 0x2
else
        echo "setup_taprio.sh failed - Invalid TX queue count"
fi