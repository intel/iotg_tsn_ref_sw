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
set -a # enable variable export

RUNSH_DEBUG_MODE="YES"
RUNSH_QDISC_DEBUG_MODE="NO"
RUNSH_RESULT_DEBUG_MODE="NO"
TSNREFSW_PACKAGE_VERSION="v0.8.28"

main() {
    #if [ $USER != "root" ]; then
    #    echo "Please run as root"
    #    exit
    #fi

    if [[ "$1" == "--version" &&  $# -eq 1 ]]; then
        echo -e "TSN REF SW version : $TSNREFSW_PACKAGE_VERSION"
        exit 0
    fi

    # Check for minimum inputs
    if [[ "$1" == "--help" || $# -lt 4 ]]; then
        echo -e "Error: invalid input. Examples on how to run:\n" \
            "Format: ./run.sh <PLAT> <IFACE> [IFACE2] <CONFIG> [ACTION]\n" \
            "For tsq*: \n" \
            "    ./run.sh ehl  eth0 tsq1a setup\n" \
            "    ./run.sh ehl  eth0 tsq1a run\n" \
            "For vs1*: \n" \
            "    ./run.sh tglu eth0 vs1a setup\n" \
            "    ./run.sh tglu eth0 vs1a run\n" \
            "For single-ethernet opcua-pkt/xdp*:\n"\
            "    ./run.sh ehl  eth0 opcua-pkt1a init\n"\
            "    ./run.sh ehl  eth0 opcua-pkt1a setup\n"\
            "    ./run.sh ehl  eth0 opcua-pkt1a run\n"\
            "For 2-port-ethernet opcua-pkt/xdp*, add a 2 to the end, and extra interface:"\
            "    ./run.sh ehl2 eth0 eth1 opcua-pkt2a init\n"\
            "    ./run.sh ehl2 eth0 eth1 opcua-pkt2a setup\n"\
            "    ./run.sh ehl2 eth0 eth1 opcua-pkt2a run\n"\
            "For tsn_ref_sw version:\n"\
            "    ./run.sh --version\n"\
            "For tsn_ref_sw help:\n"\
            "    ./run.sh --help";
        exit 1
    fi

    PLAT=$1
    IFACE=$2

    # Check for valid <PLAT>
    if [[ "$1" == "tglu" || "$1" == "ehl" || "$1" == "tglh" || "$1" == "i225" || "$1" == "adls" || "$1" == "adln" || "$1" == "rplp" || "$1" == "rpls" ]]; then
        IFACE2=""
        CONFIG=$3
        ACTION=$4
    elif [[ "$1" == "tglh2" || "$1" == "ehl2" || "$1" == "adls2" || "$1" == "rpls2" ]]; then
        IFACE2=$3
        CONFIG=$4
        ACTION=$5
    else
        LIST=$(ls -d shell/*/ | rev | cut -c 2- | rev | cut -c 7- | sort)
        LIST+=$(echo -ne " \n")
        echo -e "Run.sh invalid <PLAT>: $PLAT\n\nList of supported platforms for sh:"
        echo $LIST

        LIST=$(ls -d json/*/ | rev | cut -c 2- | rev | cut -c 6- | sort)
        LIST+=$(echo -ne " \n")
        echo -e "\nList of supported platforms for json:"
        echo $LIST
        echo -e "\nPlease run ./run.sh --help for more info."
        exit 1
    fi

    # Check for valid <IFACE>
    ip a show $IFACE up > /dev/null
    if [ $? -eq 1 ]; then echo "Error: Invalid interface $IFACE"; exit 1; fi

    if [ ! -z $IFACE2 ]; then
        ip a show $IFACE2 up > /dev/null
        if [ $? -eq 1 ]; then echo "Error: Invalid interface $IFACE2"; exit 1; fi
    fi

    # Check for valid <CONFIG>
    LIST=$(ls shell/$PLAT 2> /dev/null | rev | cut -c 8- | rev | sort)
    if [[ $LIST =~ (^|[[:space:]])"$CONFIG"($|[[:space:]]) ]] ; then
        echo "Run.sh selected: $CONFIG"
    elif [[ "(^|[[:space:]])"$CONFIG"($|[[:space:]])" =~ "opcua" ]] ; then
        echo "Run.sh detected: opcua/json config" # Do nothing
    else
        LIST+=$(echo -ne " \n")
        echo -e "Run.sh invalid <CONFIG>: $CONFIG \nAvailable configs for $PLAT:"
        printf '%s\n' "${LIST[@]}"
        exit
    fi

    # Only for debug: timesync per-run logging
    if [[ "$RUNSH_DEBUG_MODE" == "YES" && "$ACTION" == "run" ]]; then
        ts_log_start
    fi

    # Store the script's dir as variable before change dir
    LOCAL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

    # Check for log directory availability
    VAR_DIR="/var"
    LOG_DIR="log"

    if [[ ! -d $VAR_DIR/$LOG_DIR ]]; then
        echo "$VAR_DIR/$LOG_DIR does not exist or it is a broken link. Checking further."
        cd $VAR_DIR
        if [[ -L $LOG_DIR && ! -e $LOG_DIR ]]; then
            LOGDIR_LINK=$(readlink $LOG_DIR)
            echo "$LOG_DIR is pointing to a broken link : $LOGDIR_LINK"
            mkdir -p $LOGDIR_LINK
            [[ -d $LOGDIR_LINK ]] && echo "$LOGDIR_LINK has been created."
        else
            mkdir -p $LOG_DIR
            [[ -d $LOG_DIR ]] && echo "$LOG_DIR has been created."
        fi
    else
            echo "$VAR_DIR/$LOG_DIR folder for logging exists."
    fi

    # Return to TSN Ref Sw App directory for script execution
    cd $LOCAL_DIR

    # Execute: redirect to opcua if opcua config, otherwise execute shell scripts
    CHECK=$(echo $CONFIG | cut -c -5 )
    if [ "$CHECK" == "opcua" ]; then
        ./json/opcua-run.sh $PLAT $IFACE $IFACE2 $CONFIG $ACTION

    elif [[ "$ACTION" == "setup" || "$ACTION" == "init" ]]; then
        export PLAT=$PLAT
        ./shell/setup-$CONFIG.sh $IFACE

    elif [ "$ACTION" == "run" ]; then
        export PLAT=$PLAT
        ./shell/$CONFIG.sh $IFACE

    else
        echo "Error: run.sh invalid commands. Please run ./run.sh --help for more info."
        exit 1
    fi

    # Only for debug: timesync per-run logging
    if [[ "$RUNSH_DEBUG_MODE" == "YES" && "$ACTION" == "run" ]]; then
        ts_log_stop_n_report
    fi

    if [[ "$RUNSH_DEBUG_MODE" == "YES" && "$ACTION" == "setup" ]]; then
        save_board_info > board_info.txt
    fi
}

ts_log_start(){
    cat /var/log/ptp4l.log >> /var/log/total_ptp4l.log
    cat /var/log/phc2sys.log >> /var/log/total_phc2sys.log

    echo -n "" > /var/log/ptp4l.log
    echo -n "" > /var/log/phc2sys.log
    echo -n "" > /var/log/captured_ptp4l.log
    echo -n "" > /var/log/captured_phc2sys.log
}

ts_log_stop_n_report(){
    num_ptp_lines=$(cat /var/log/captured_ptp4l.log | wc -l)
    if [[ num_ptp_lines -gt 2 ]]; then
        grep -vaP '[\0\200-\377]' /var/log/captured_ptp4l.log > /var/log/temp_ptp4l.log
    else
        grep -vaP '[\0\200-\377]' /var/log/ptp4l.log > /var/log/temp_ptp4l.log
    fi
    num_phc_lines=$(cat /var/log/captured_phc2sys.log | wc -l)
    if [[ num_phc_lines -gt 2 ]]; then
        grep -vaP '[\0\200-\377]' /var/log/captured_phc2sys.log > /var/log/temp_phc2sys.log
    else
        grep -vaP '[\0\200-\377]' /var/log/phc2sys.log > /var/log/temp_phc2sys.log
    fi

    echo -e "---------------------------------------------------------------------------------------"
    PHC2SYS_MIN=$(grep offset /var/log/temp_phc2sys.log | sort -nk 5 | head -n 1 | awk '{print $5}')
    PHC2SYS_MAX=$(grep offset /var/log/temp_phc2sys.log | sort -nk 5 | tail -n 1 | awk '{print $5}')
    if [[ ! -z $PHC2SYS_MAX && ! -z $PHC2SYS_MIN ]]; then
        echo -e "PHC2SYS offset" \
            "\tmin\t $PHC2SYS_MIN" \
            "\n\t\tmax\t $PHC2SYS_MAX"

        echo -e "\tdelay " \
            "\tmin\t $(grep delay /var/log/temp_phc2sys.log | sort -nk 10 | head -n 1 | awk '{print $10}')" \
            "\n\t\tmax\t $(grep delay /var/log/temp_phc2sys.log | sort -nk 10 | tail -n 1 | awk '{print $10}')"

        echo -e "\twaiting:         $(grep -i waiting /var/log/temp_phc2sys.log | wc -l)"
        echo -e "\tfailed:          $(grep failed /var/log/temp_phc2sys.log | wc -l)"
    fi

    PTP_RMS_MIN=$(grep rms /var/log/temp_ptp4l.log | sort -nk 3 | head -n 1 | awk '{print $3}')
    PTP_RMS_MAX=$(grep rms /var/log/temp_ptp4l.log | sort -nk 3 | tail -n 1 | awk '{print $3}')
    if [[ ! -z $PTP_RMS_MIN && ! -z $PTP_RMS_MAX ]]; then
        echo -e "PTP4L   rms   "  \
        "\tmin\t $PTP_RMS_MIN" \
        "\n\t\tmax\t $PTP_RMS_MAX"
        echo -e "\tMCLK_SELECTED:   $(grep MASTER_CLOCK_SELECTED /var/log/temp_ptp4l.log | wc -l)"
        echo -e "\tINIT_COMPLETE:   $(grep FAULT_DETECTED /var/log/temp_ptp4l.log | wc -l)"
        echo -e "\tFAULT_DETECTED:  $(grep FAULT_DETECTED /var/log/temp_ptp4l.log | wc -l)"
        echo -e "\tUNCALIBRATED:    $(grep UNCALIBRATED /var/log/temp_ptp4l.log | wc -l)"
        echo -e "\tlink down:       $(grep "link down" /var/log/temp_ptp4l.log | wc -l)"
        echo -e "\ttimed out:       $(grep "timed out" /var/log/temp_ptp4l.log | wc -l)"
        echo -e "\ttemporal:       $(grep "temporal" /var/log/temp_ptp4l.log | wc -l)"
        echo -e "\tUTCoffset:       $(grep "updating UTC offset to 0" /var/log/temp_ptp4l.log | wc -l)\n"
    fi

    if [[ "$RUNSH_QDISC_DEBUG_MODE" == "YES" ]]; then
        if [[ "$CONFIG" == "opcua-pkt2a" || "$CONFIG" == "opcua-pkt3a" ||
            "$CONFIG" == "opcua-xdp2a" || "$CONFIG" == "opcua-xdp3a" ]]; then
            tc -s qdisc show dev $IFACE
        elif [[ "$CONFIG" == "opcua-pkt2b" || "$CONFIG" == "opcua-pkt3b" ||
                "$CONFIG" == "opcua-xdp2b" || "$CONFIG" == "opcua-xdp3b" ]]; then
            tc -s qdisc show dev $IFACE2
        else
            echo "" # Nothing
        fi
    fi
}

get_taprio_setting(){
    local IFACE="$1"
    local OUTPUTFILE="$2"
    if [ ! -z "$IFACE" ]; then
        echo -e "\nInterface: $IFACE" >> "${OUTPUTFILE}"
        TAPRIO_SETTING=$(tc -s qdisc show dev "$IFACE" | \
                        sed '/qdisc taprio/,/index 1/!d')
        if [[ ! -z "$TAPRIO_SETTING" ]]; then
            echo -e "$TAPRIO_SETTING" >> "${OUTPUTFILE}"
        else
            echo -e "No taprio set for this interface." >> "${OUTPUTFILE}"
        fi
    fi
}

save_gcl_info() {
    rm -rf gclinfo.txt
    touch gclinfo.txt
    echo -n "" > gclinfo.txt
    get_taprio_setting "$IFACE" gclinfo.txt
    get_taprio_setting "$IFACE2" gclinfo.txt
    echo "---------------------------------------------------------------------------------------"
    echo "GATE CONFIGURATION LIST:"
    cat gclinfo.txt
}

save_board_info(){
        echo -e "\n\nBIOS Version: " $(dmidecode -s bios-version)
        echo "Kernel Version: " $(uname -r)
        echo "Kernel Build Date: " $(uname -v)
        echo -e "Kernel Parameter: \n" $(cat /proc/cmdline)

        echo "CPU Info:"
        grep "MHz" /proc/cpuinfo

        echo "Memory Info:"
        free -mt

        echo "Storage Info:"
        df -h

        echo "Ethernet Info:"
        ethtool -i $IFACE | head -n 1
        cat /proc/interrupts | grep $IFACE

        if [ ! -z $IFACE2 ]; then
            ethtool -i $IFACE2 | head -n 1
            cat /proc/interrupts | grep $IFACE2
        fi
}

main "$@"

set +a # Disable variable export
