#!/bin/sh

#if [ $USER != "root" ]; then
#    echo "Please run as root"
#    exit
#fi

if [ -z $1 ]; then
    echo "Please enter interface e.g. ./run.sh eth0 tsq1a"
    exit
fi
IFACE=$1

if [ -z $2 ]; then
    echo "Please enter config e.g. ./run.sh eth0 tsq1a"
    exit
else
    CONFIG=$2
    # Parse and check if its a valid base/config
    LIST=$(ls  scripts/ | grep 'tsq\|vs\|opcua' | rev | cut -c 4- | rev | sort)
    LIST+=$(echo -ne " \n")
    if [[ $LIST =~ (^|[[:space:]])"$CONFIG"($|[[:space:]]) ]] ; then
        echo "Selected config: $CONFIG"
    else
        echo -e "Invalid config selected: $CONFIG \nAvailable configs:"
        printf '%s\n' "${LIST[@]}"
        exit
    fi
fi

# Run something else first.
if [ -n $3 ]; then
    case "$3" in
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
./scripts/$CONFIG.sh $IFACE

