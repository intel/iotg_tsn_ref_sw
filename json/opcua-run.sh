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
Examples: ./opcua-run.sh ehl  eth0 opcua-pkt1a init
          ./opcua-run.sh ehl2 eth0 eth1 opcua-pkt2a setup
          ./opcua-run.sh ehl2 eth0 eth1 opcua-pkt3a run
    "
    exit 0
fi

if [[ "$2" = "" ]]; then
    echo "Please specify an interface name"
    exit 1
fi

PLAT="$1"
IFACE="$2"

if [[ "$PLAT" = "ehl2" || "$PLAT" = "tglh2" || "$PLAT" = "adl2" || "$PLAT" = "rpl2" ]];then
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

ts_log_start(){
    cat /var/log/ptp4l.log >> /var/log/total_ptp4l.log
    cat /var/log/phc2sys.log >> /var/log/total_phc2sys.log

    echo -n "" > /var/log/ptp4l.log
    echo -n "" > /var/log/phc2sys.log
    echo -n "" > /var/log/captured_ptp4l.log
    echo -n "" > /var/log/captured_phc2sys.log
}

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

if [[ "$PLAT" = "ehl2" || "$PLAT" = "tglh2" || "$PLAT" = "adl2" || "$PLAT" = "rpl2" ]];then
    sed -i -e "s/_PREPROCESS_STR_2nd_interface/$IFACE2/gi" $NEW_JSON $NEW_TSN_JSON
fi

KERNEL_VER=$(uname -r | cut -d'.' -f1-2)

# Work around for 5.10 and above kernel due to interface reset after xdp init
# This means, the setup will only take place after the interface is up again (after entering XDP mode)
SKIP_SETUP="n"
if [[ $KERNEL_VER == 5.1* ]]; then
    if [[ ( "$CONFIG" == "opcua-xdp2a" || "$CONFIG" == "opcua-xdp2b" || "$CONFIG" == "opcua-xdp3a" || "$CONFIG" == "opcua-xdp3b" ) ]]; then
        SKIP_SETUP="y"
    fi
fi

case "$MODE" in

    # Set the static ip and mac address only. using opcua-*.cfg
    init)
        case "$CONFIG" in
            # .config: A/B used for single-trip, C/D added for round-trip
            #
            #   opcua-A --------------> opcua-B
            #                             |
            #                             V
            #   opcua-D <-------------- opcua-C
            #
            #   Board A                 Board B
            #
            opcua-*a)
                init_interface $PLAT $IFACE $DIR/$PLAT/opcua-A.config
                if [[ ! -z "$IFACE2" ]];then
                    init_interface $PLAT $IFACE2 $DIR/$PLAT/opcua-D.config;
                fi
                ;;
            opcua-*b)
                init_interface $PLAT $IFACE $DIR/$PLAT/opcua-B.config
                if [[ ! -z "$IFACE2" ]];then
                    init_interface $PLAT $IFACE2 $DIR/$PLAT/opcua-C.config;
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

    # Run setup using opcua*-tsn.json
    setup) 
        rm -f $DIR/../$IPERF3_GEN_CMD

        python3 $DIR/gen_setup.py $PLAT "$NEW_TSN_JSON"
        if [ $? -ne 0 ]; then
            echo "gen_setup.py returned non-zero. Abort" && exit
        fi

        # Work around for 5.10 and above kernel due to reset after xdp init
        if [[ "$SKIP_SETUP" == "y" ]]; then
                echo "[KERNEL_5.1x_XDP] gen_setup.py is parsed, ./setup-generated.sh generated but will only run after opcua server starts."
                exit 0
        fi

        sh ./setup-generated.sh

        # Extra Delay to stabilize gPTP
        PTP_PROCESSES=$(pgrep -c ptp4l)
        if [ "$PTP_PROCESSES" -ne 0 ]; then
            echo "Wait 60 sec for gPTP to sync properly after setting the queues."
            sleep 60
        fi

        exit 0
        ;;

     run)
         # Execute as written below
        ;;

    *)
        echo "Invalid mode $MODE" && exit 1
        ;;
esac

# If the client iperf3 generated cmd file exists
# In the case of XDP in 5.10 and above, we will only start iperf after the interface restart and setup is finished
if [[ "$SKIP_SETUP" == "n" ]]; then
    if [[ -f "$IPERF3_GEN_CMD" ]]; then
        CMD="$(cat $DIR/../$IPERF3_GEN_CMD)"
        echo "Running: $CMD"
        $CMD &
    else
        echo "Not running iperf3."
    fi
fi

# Create soft link to output file in /tmp
OUTPUT_FILE=$(grep -s subscriber_output_file $NEW_JSON | awk '{print $2}' | sed 's/,//' | sed 's/\"//g')
if [[ ! -z $OUTPUT_FILE ]]; then
    rm -f ./$OUTPUT_FILE
    TEMP_DIR=$(grep -s temp_file_dir $NEW_JSON | awk '{print $2}' | sed 's/,//' | sed 's/\"//g')
    if [[ -z $TEMP_DIR ]]; then
        echo "temp_file_dir is not defined in $NEW_JSON. Falling back to /tmp !!!"
        TEMP_DIR="/tmp"
    fi

    if [[ ! -d "$TEMP_DIR" ]]; then
        echo "Output temp dir for output file: $TEMP_DIR - inexistent. Exiting."
        exit 1
    else
        echo -n "" > $TEMP_DIR/$OUTPUT_FILE
        ln -sfv $TEMP_DIR/$OUTPUT_FILE .
    fi
fi

# Workaround for delays spikes after the first init-setup
if [[ "$CONFIG" == "opcua-pkt2a" || "$CONFIG" == "opcua-pkt3a" ]]; then
    echo "Workaround A to flush queue"
    sleep 5
    if [[ "$PLAT" = "ehl2" ]]; then
        ./txrx-tsn -Pti $IFACE -q 2 -n 10000 -y 200000 > someTX.txt &
    elif [[ "$PLAT" = "tglh2" || "$PLAT" = "adl2" || "$PLAT" = "rpl2" ]]; then
        ./txrx-tsn -Pti $IFACE -q 1 -n 10000 -y 200000 > someTX.txt &
    fi
    sleep 5 && pkill txrx-tsn

elif [[ "$CONFIG" == "opcua-pkt2b" || "$CONFIG" == "opcua-pkt3b" ]]; then
    echo "Workaround B to flush queue"
    if [[ "$PLAT" = "ehl2" ]]; then
        ./txrx-tsn -Pri $IFACE -q 2 > someRX.txt &
    elif [[ "$PLAT" = "tglh2" || "$PLAT" = "adl2" || "$PLAT" = "rpl2" ]]; then
        ./txrx-tsn -Pri $IFACE -q 1 > someRX.txt &
    fi
    sleep 10 && pkill txrx-tsn

else
    echo "" # Nothing
fi

# Extra Delay to stabilize test environment
echo "Wait 20 sec for test environment to stabilize before running opcua server."
sleep 20

# Execute the server and pass it opcua-*.json
./opcua-server "$NEW_JSON" &

OPCUA_PID=$!
RETVAL_OPCUA=$?

# all settings are lost after xdp init on 5.10 and above.
# run setup after running opcua-server
if [[ "$SKIP_SETUP" == "y" ]]; then

        echo "[KERNEL_5.1x_XDP] Reapply init after interface reset"
        sleep 10

        case "$CONFIG" in
            # .config: A/B used for single-trip, C/D added for round-trip
            #
            #   opcua-A --------------> opcua-B
            #                             |
            #                             V
            #   opcua-D <-------------- opcua-C
            #
            #   Board A                 Board B
            #
            opcua-*a)
                init_interface $PLAT $IFACE $DIR/$PLAT/opcua-A.config "n"
                if [[ ! -z "$IFACE2" ]];then
                    init_interface $PLAT $IFACE2 $DIR/$PLAT/opcua-D.config "y";
                fi
                ;;
            opcua-*b)
                init_interface $PLAT $IFACE $DIR/$PLAT/opcua-B.config "y"
                if [[ ! -z "$IFACE2" ]];then
                    init_interface $PLAT $IFACE2 $DIR/$PLAT/opcua-C.config "n";
                fi
                ;;
            "")
                ;&
            *)
                echo "Invalid config $CONFIG"
                exit 1
                ;;
        esac

        echo "[KERNEL5.1x_XDP] Run previously generated ./setup-generated.sh"
        sh ./setup-generated.sh

        if [[ -f "$IPERF3_GEN_CMD" ]]; then
            CMD="$(cat $DIR/../$IPERF3_GEN_CMD)"
            echo "Running: $CMD"
            $CMD &
        else
            echo "Not running iperf3."
        fi

        # Extra Delay to stabilize gPTP
        PTP_PROCESSES=$(pgrep -c ptp4l)
        if [ "$PTP_PROCESSES" -ne 0 ]; then
            echo "Wait 60 sec for gPTP to sync properly after setting the queues."
            sleep 60
        fi

        echo "[KERNEL5.1x_XDP] Setup Done. Opcua transmision will start shortly."

        ts_log_start
fi

while ps -p $OPCUA_PID >/dev/null
do
    sleep 2
done

echo "Transfer Complete"

# Kill iperf3 client. Leave iperf3 server running."
IPERF3_CLI_PID=$(pgrep iperf3 -a | grep "iperf3 -c" | awk '{print $1}')
if [[ ! -z "$IPERF3_CLI_PID" ]];then
    kill -9 $IPERF3_CLI_PID
fi

if [[ "$SKIP_SETUP" == "y" ]]; then
    echo "[Kernel5.1x_XDP] De-Activate napi busy polling for inf:$IFACE"
    echo 0 > /sys/class/net/$IFACE/gro_flush_timeout
    echo 0 > /sys/class/net/$IFACE/napi_defer_hard_irqs
    if [[ ! -z "$IFACE2" ]];then
        echo "[Kernel5.1x_XDP] De-Activate napi busy polling for 2nd inf:$IFACE2"
        echo 0 > /sys/class/net/$IFACE2/gro_flush_timeout
        echo 0 > /sys/class/net/$IFACE2/napi_defer_hard_irqs
    fi
    sleep 5
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
    save_gcl_info
    exit 0
fi

stop_if_empty "$TYPE-rxtstamps.txt"

echo "---------------------------------------------------------------------------------------"
CONFIGNUM="$(echo $CONFIG | cut -c 10)"
if [ $CONFIGNUM -lt 2 ]; then
    calc_rx_u2u "$TYPE-rxtstamps.txt"
    calc_stddev_u2u FALSE "$TYPE-rxtstamps.txt"
    calc_rx_duploss "$TYPE-rxtstamps.txt" "$NEW_JSON"
    calc_tbs_stddev "$TYPE-rxtstamps.txt"
elif [ $CONFIGNUM -lt 4 ]; then
    calc_return_u2u "$TYPE-rxtstamps.txt"
    calc_stddev_u2u YES "$TYPE-rxtstamps.txt"
    calc_return_duploss "$TYPE-rxtstamps.txt" "$NEW_JSON"
    calc_tbs_stddev "$TYPE-rxtstamps.txt"
else
    # echo "Nothing to calculate"
    save_gcl_info
    exit 0
fi

save_gcl_info

save_result_files $CONFIG $PLAT "$NEW_JSON" #this file's name aka config.

GNUPLOT_PATH=$(which gnuplot)

if [[ -z $GNUPLOT_PATH ]]; then
    echo "INFO: gnuplot is not available in this system. No graph will be created."
    exit 0
fi

if [[ "$TYPE" == "afxdp" ]]; then
	gnuplot -e "FILENAME='afxdp-traffic.txt'; YMAX=2000000; PLOT_TITLE='Transmission latency from TX User-space to RX User-space (AFXDP)'" $DIR/../common/latency_single.gnu -p 2> /dev/null &
elif [[ "$CONFIG" == "opcua-pkt1b" ]]; then
    # Plotting for single trip
    gnuplot -e "FILENAME='afpkt-traffic.txt'; YMAX=10000000; PLOT_TITLE='Transmission latency from TX User-space to RX User-space (AF-PACKET - single trip)'" $DIR/../common/latency_single.gnu -p 2> /dev/null &
else
    # Plotting for return trip
    gnuplot -e "FILENAME='afpkt-traffic.txt'; YMAX=10000000; PLOT_TITLE='Transmission latency from TX User-space to RX User-space (AF-PACKET - return trip)'" $DIR/../common/latency_single.gnu -p 2> /dev/null &
fi

while [[ ! -s plot_pic.png ]] && [[ $RETRY -lt 30 ]]; do sleep 5; let $(( RETRY++ )); done
cp plot_pic.png results-$ID/$PLAT-plot-$CONFIG-$KERNEL_VER-$NUMPKTS-$INTERVAL-$IDD.png # Backup

exit 0
