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

if [[ "$1" = "-h" ]] || [[ "$1" = "" ]]; then
    echo "\
Usage: opcua-run.sh <PLAT> <interface> [interface2] <CONFIG> [mode]
Example: ./opcua-run.sh ehl  eth0 opcua-pkt1a init
         ./opcua-run.sh ehl2 eth0 eth1 opcua-pkt2a setup
    "
    exit 0
fi

if [[ "$2" = "" ]]; then
    echo "Please specify an interface name"
    exit 1
fi

PLAT="$1"
IFACE="$2"

if [[ "$PLAT" = "ehl2" ]];then
    IFACE2="$3"
    CONFIG="$4"
    MODE="$5"
else
    CONFIG="$3"
    MODE="$4"
fi

# Get directory of current script
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source $DIR/helpers.sh
JSONDIR="$DIR/$PLAT/"

# Parse and check if its valid: e.g. opcua-pkt-1a
LIST=$(ls $JSONDIR | grep 'opcua' | grep 'json.i' | rev | cut -c 8- | rev | sort)
if [[ $LIST =~ (^|[[:space:]])"$CONFIG"($|[[:space:]]) ]] ; then
    echo "Selected json-config: $CONFIG"
else
    echo -e "Invalid json-config selected: $CONFIG \nAvailable opcua json-configs:"
    printf '%s\n' "${LIST[@]}"
    exit
fi

#Iperf3 generated command file from gen_setup.py using json parameters for iperf3 client
IPERF3_GEN_CMD="iperf3-gen-cmd.sh"

#Selected json file's absolute path
INTERIM_JSON="$JSONDIR/$CONFIG.json.i"
INTERIM_TSN_JSON="$JSONDIR/$CONFIG-tsn.json.i"

#New filename without .i
NEW_JSON=${INTERIM_JSON%.i}
NEW_TSN_JSON=${INTERIM_TSN_JSON%.i}

cp -f $INTERIM_JSON $NEW_JSON
cp -f $INTERIM_TSN_JSON $NEW_TSN_JSON

#Replace interface on .json
sed -i -e "s/_PREPROCESS_STR_interface/$IFACE/gi" $NEW_JSON $NEW_TSN_JSON

if [[ "$PLAT" = "ehl2" || "$PLAT" = "tgl2" ]];then
    sed -i -e "s/_PREPROCESS_STR_2nd_interface/$IFACE2/gi" $NEW_JSON $NEW_TSN_JSON
fi

case "$MODE" in
    init) # Set the static ip and mac address only
        case "$CONFIG" in
            opcua-*a)
                setup_sp1a $IFACE
                if [[ ! -z "$IFACE2" ]];then
                    setup_sp2a $IFACE2
                    #Overwrite IFACE1 previous map to use multi-stream map
                    set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_8tx_8rx-multi.map
                fi
                ;;
            opcua-*b)
                setup_sp1b $IFACE
                set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_8tx_8rx-multi.map
                if [[ ! -z "$IFACE2" ]];then
                    setup_sp2b $IFACE2
                    #Overwrite IFACE1 previous map to use multi-stream map
                    set_irq_smp_affinity $IFACE $DIR/../scripts/irq_affinity_4c_8tx_8rx-multi.map
                fi
                ;;
            "")
                ;&
            *)
                echo "Invalid config $CONFIG"
                exit 1
                ;;
        esac
        exit 0
        ;;
    setup) # Run setup using *-tsn.json, and then exit
        #Remove existing iperf3 generated command file
        rm -f $DIR/../$IPERF3_GEN_CMD

        python3 $DIR/gen_setup.py "$NEW_TSN_JSON"
        if [ $? -ne 0 ]; then
            echo "gen_setup.py returned non-zero. Abort" && exit
        fi
        sh ./setup-generated.sh
        exit 0
        ;;
    run) # Proceed without running init or setup
        ;;

    "") # Default if no MODE specified, run init, setup and start opcua-server
        case "$CONFIG" in
            opcua-*a)
                setup_sp1a $IFACE
                if [[ ! -z "$IFACE2" ]];then
                    setup_sp2a $IFACE2
                fi
                ;;
            opcua-*b)
                setup_sp1b $IFACE
                if [[ ! -z "$IFACE2" ]];then
                    setup_sp2a $IFACE2
                fi
                ;;
            "")
                ;&
            *)
                echo "Invalid config $CONFIG"
                exit 1
                ;;
        esac
        ;& #fallthru
    setup-run)
        #Remove existing iperf3 generated command file
        [[ -f "$DIR/../$IPERF3_GEN_CMD" ]] && rm -f $DIR/../$IPERF3_GEN_CMD

        python3 $DIR/gen_setup.py "$NEW_TSN_JSON"
        if [ $? -ne 0 ]; then
            echo "gen_setup.py returned non-zero. Abort" && exit
        fi
        sh ./setup-generated.sh
        ;;
    *)
        echo "Invalid mode $MODE" && exit 1
        ;;
esac

# If the client iperf3 generated cmd file exists
if [[ -f "$IPERF3_GEN_CMD" ]]; then
    CMD="$(cat $DIR/../$IPERF3_GEN_CMD)"
    echo "Running: $CMD"
    $CMD &
else
    echo "Not running iperf3."
fi

./opcua-server "$NEW_JSON"

RETVAL_OPCUA=$?

# Kill iperf3 client. Leave iperf3 server running."
IPERF3_CLI_PID=$(pgrep iperf3 -a | grep "iperf3 -c" | awk '{print $1}')
if [[ ! -z "$IPERF3_CLI_PID" ]];then
    kill -9 $IPERF3_CLI_PID
fi

if [[ "$RETVAL_OPCUA" -ne 0 ]]; then
    echo "opcua-server returned non-zero. Abort" && exit
elif [[ "$CONFIG" == "opcua-pkt0b"  || "$CONFIG" == "opcua-pkt1b" ||
        "$CONFIG" == "opcua-pkt2a"  || "$CONFIG" == "opcua-pkt3a" ||
        "$CONFIG" == "opcua-pkt4a"  || "$CONFIG" == "opcua-pkt5a" ]]; then
    TYPE="afpkt"
elif [[ "$CONFIG" == "opcua-xdp0b"  || "$CONFIG" == "opcua-xdp1b" ||
        "$CONFIG" == "opcua-xdp2a"  || "$CONFIG" == "opcua-xdp3a" ||
        "$CONFIG" == "opcua-xdp4a"  || "$CONFIG" == "opcua-xdp5a" ]]; then
    TYPE="afxdp"
else
    # echo "Nothing to plot"
    exit 0
fi

stop_if_empty "$TYPE-rxtstamps.txt"

CONFIGNUM="$(echo $CONFIG | cut -c 10)"
if [ $CONFIGNUM -lt 2 ]; then
    calc_rx_u2u "$TYPE-rxtstamps.txt"
    calc_rx_duploss "$TYPE-rxtstamps.txt" "$NEW_JSON"
elif [ $CONFIGNUM -lt 4 ]; then
    calc_return_u2u "$TYPE-rxtstamps.txt"
    calc_return_duploss "$TYPE-rxtstamps.txt" "$NEW_JSON"
else
    # echo "Nothing to calculate"
    exit 0
fi

save_result_files $CONFIG $NUMPKTS $SIZE $INTERVAL #this file's name aka config.

if [[ "$TYPE" == "afxdp" ]]; then
	gnuplot -e "FILENAME='afxdp-traffic.txt'; YMAX=2000000; PLOT_TITLE='Transmission latency from TX User-space to RX User-space (AFXDP)'" $DIR/../scripts/latency_single.gnu -p 2> /dev/null &
elif [[ "$CONFIG" == "opcua-pkt1b" ]]; then
    # Plotting for single trip
    gnuplot -e "FILENAME='afpkt-traffic.txt'; YMAX=10000000; PLOT_TITLE='Transmission latency from TX User-space to RX User-space (AF-PACKET - single trip)'" $DIR/../scripts/latency_single.gnu -p 2> /dev/null &
else
    # Plotting for return trip
    gnuplot -e "FILENAME='afpkt-traffic.txt'; YMAX=10000000; PLOT_TITLE='Transmission latency from TX User-space to RX User-space (AF-PACKET - return trip)'" $DIR/../scripts/latency_single.gnu -p 2> /dev/null &
fi

while [[ ! -s plot_pic.png ]]; do sleep 5; done
cp plot_pic.png results-$ID/plot-$CONFIG-$NUMPKTS-$SIZE-$INTERVAL-$IDD.png # Backup
