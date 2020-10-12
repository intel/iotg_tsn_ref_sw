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

#if [ $USER != "root" ]; then
#    echo "Please run as root"
#    exit
#fi

if [ -z $1 ]; then
    echo "For tsq*/xdp*/pkt* use:
        ./run.sh eth0 tsq1a
For single-ethernet opcua-pkt/xdp* use:
        ./run.sh ehl  eth0 opcua-pkt1a init
For double-ethernet opcua-pkt/xdp* use:
        ./run.sh ehl2 eth0 eth1 opcua-pkt2a init"
    exit
fi

if [[ "$1" == "tgl" || "$1" == "ehl" ]]; then
    PLAT=$1
    IFACE=$2
    IFACE2=""
    CONFIG=$3
    MODE=$4
elif [[ "$1" == "tgl2" || "$1" == "ehl2" ]]; then
    PLAT=$1
    IFACE=$2
    IFACE2=$3
    CONFIG=$4
    MODE=$5
else
    IFACE=$1
    CONFIG=$2
    ETC=$3
fi

if [ -z $CONFIG ]; then
    echo "For tsq*/xdp*/pkt* use:
        ./run.sh eth0 tsq1a
For single-ethernet opcua-pkt/xdp* use:
        ./run.sh ehl  eth0 opcua-pkt1a init
For double-ethernet opcua-pkt/xdp* use:
        ./run.sh ehl2 eth0 eth1 opcua-pkt2a init"
    exit
else
    # Parse and check if its a valid base/config
    LIST=$(ls  scripts/ | grep 'tsq\|vs' | rev | cut -c 4- | rev | sort)
    LIST+=$(echo -ne " \n")
    if [[ $LIST =~ (^|[[:space:]])"$CONFIG"($|[[:space:]]) ]] ; then
        echo "Selected config: $CONFIG"
    elif [[ "(^|[[:space:]])"$CONFIG"($|[[:space:]])" =~ "opcua" ]] ; then
        echo "opcua config detected"
    else
        echo -e "Invalid config selected: $CONFIG \nAvailable configs:"
        printf '%s\n' "${LIST[@]}"
        exit
    fi
fi

# Run something else first.
if [ -n $ETC ]; then
    case "$ETC" in
        clock)
            echo "[RUN.SH] Running clock-setup.sh"
            ./scripts/clock-setup.sh $IFACE
            ;;
        setup*)
            echo "[RUN.SH] Running setup-$CONFIG, then $CONFIG"
            ./scripts/setup-$CONFIG.sh $IFACE
            ;;
        "") #Do nothing
            ;;
        *)
            echo "Invalid third option: $3"
            exit
            ;;
    esac
fi

#Execute the specific config script, using their own defaults.
CHECK=$(echo $CONFIG | cut -c -5 )
if [ $CHECK == "opcua" ]; then
        # For json types like opcua-pkt*, opcua-xdp*
        ./json/opcua-run.sh $PLAT $IFACE $IFACE2 $CONFIG $MODE
else
        # For shell types like tsq*, xdp*, pkt*, vs* - use script defaults
        ./scripts/$CONFIG.sh $IFACE
fi
