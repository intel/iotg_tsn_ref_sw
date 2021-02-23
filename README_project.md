# The project

## Scope

- User shall use two units of any platforms listed in [Compatibility](README.md#Compatibility).
- User shall be using the platform's respective BSP and its provided ingredients.
- User shall be using 2-port for round-trip cases.
- User shall be using integrateed Ethernet controller (specifically one with stmmac driver)

## Objective

This project has scripts and code to enable/disable specific TSN features which
can be used as a reference.

Table of TSN features used in each example:

| Config/Example  | PHC2SYS | PTP4L | TAPRIO | ETF | Ports |  Data Flow   | XDP ZC | IPERF |
| --------------- | ------- | ----- | ------ | --- | ----- | ------------ | ------ | ----- |
| tsq1            | √*      | √     |        |     | 1     | single-trip* |        |       |
| vs1             | √       | √     | √      | √   | 1     | single-trip  | √*     | √     |
| opcua-pkt1      | √       | √     | √      | √   | 1     | single-trip  |        |       |
| opcua-pkt2      | √       | √     | √      | √   | 2     | round-trip   |        |       |
| opcua-pkt3      | √       | √     | √      | √   | 2     | round-trip   |        | √     |
| opcua-xdp2      | √       | √     | √      | √   | 2     | round-trip   | √      |       |
| opcua-xdp3      | √       | √     | √      | √   | 2     | round-trip   | √      | √     |

Notes:
- \* TSQ1 shuts down phc2sys after initializing to enable EXTTS
- \* TSQ1 data flows into one listener from two talkers
- \* VS1 XDP Zero-Copy is used in the 2nd phase only

## Design

Design considerations built-into the apps and scripts. (Specifically for
TXRX-TSN & OPCUA-SERVER)

### Platform specificity

#### Queue count

There are 2 versions of the integrated controller, differentiated only by the
number of queues it has (6rx/4tx or 8rx/8tx).

For example, TGL-U/H is 6rx/4tx and EHl is 8rx/tx.

Note: some controllers, say i210/i225, have "combined" queues which may require
different handling (not the scope of this project).

#### Integrated Ethernet controller specific constraints.

- Only the upper half TX queues can use ETF (e.g. If total 8 tx, only q4,5,6,7)

- All RX traffic is routed to Q1 by default, all other RX queues not used unless
  RX queue-steering (e.g. VLAN priority) is configured.

- XDP Zero-Copy has its own queue numbering. Only the upper half of TX queues and the
  lower half of RX queues is XDP-compatible. For example:

    TX & RX queue hardware mapping normally:

    |         | HW Q0 | HW Q1 | HW Q2 | HW Q3 | HW Q4 | HW Q5 | HW Q6 | HW Q7 |
    | ------- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- |
    | TX & RX | Q0    | Q1    | Q2    | Q3    | Q4    | Q5    | Q6    | Q7    |

    TX & RX queue hardware mapping when in AF XDP ZC mode (this applies to EHL):

    |     | HW Q0  | HW Q1  | HW Q2  | HW Q3  | HW Q4  | HW Q5  | HW Q6  | HW Q7  |
    | --- | ------ | ------ | ------ | ------ | ------ | ------ | ------ | ------ |
    | TX  | Q0     | Q1     | Q2     | Q3     | XDP Q0 | XDP Q1 | XDP Q2 | XDP Q3 |
    | RX  | XDP Q0 | XDP Q1 | XDP Q2 | XDP Q3 | Q4     | Q5     | Q6     | Q7     |

    When a queue is used with XDP, all packets (including ping etc) on that queue
    will be forwarded to the XDP application assigned to that queue. Hence, Q0
    should be be used for XDP.

    For example with TGL-U/H, we are only using 4 TX and 4 RX:

    |     | HW Q0  | HW Q1  | HW Q2  | HW Q3  |
    | --- | ------ | ------ | ------ | ------ |
    | TX  | Q0     | Q1     | XDP Q0 | XDP Q1 |
    | RX  | XDP Q0 | XDP Q1 | Q3     | Q4     |

### Optimization

Fulfilling platform specifics may allow data to start flowing, but that doesn't
mean it is flowing efficiently and consistently. The following section is a
*partial example* to point out what could/should be looked into, to ensure
the platform can process tasks as deterministic as possible.

#### BIOS level

Enable TCC mode, disable TurboBoost speeds, disable cstates.

#### Kernel/GRUB boot parameter level

Add isolcpu & irq_affinity parameters to create isolated cores to assign real-time tasks into,
disable debug options like kmemleak.

#### Linux task-scheduling level

With isolcpu, you can designate tasks to run on "rt" isolated cores. In this example,
we isolated CPU1, 2 & 3. CPU 0 is the general-purpose core. "Real-time" tasks can
then be assinged to each CPU as follows.

```
EHL 1 port(1-way latency) and 2 port(2-way latency) configuration:
+---------+---------+---------+---------+---------+
| EHL     |  CPU0   |  CPU1   |  CPU2   |  CPU3   |
|         |         |         |         |         |
+=========+=========+=========+=========+=========+
| App     | iperf   | PTP     | TX      | RX      |
|         | others  |         |         |         |
|         |         |         |         |         |
+---------+---------+---------+---------+---------+
| XDPQ    |         |         | XDP TXQ2| XDP RXQ2|
|         |         |         |         |         |
+---------+---------+---------+---------+---------+
| IRQ/TXQ | TX0 RX0 | TX1 RX1 | TX6     | RX2     |
|         |         |         |         |         |
+---------+---------+---------+---------+---------+

TGL-U 1 port(1-way latency test) configuration:
+---------+---------+---------+---------+---------+
|  TGLU   |  CPU0   |  CPU1   |  CPU2   | CPU3    |
|         |         |         |         |         |
+=========+=========+=========+=========+=========+
| App     | iperf   | PTP     | TX/RX   |         |
|         | others  |         |         |         |
|         |         |         |         |         |
+---------+---------+---------+---------+---------+
| XDPQ    |         |         | XDP TXQ1|         |
|         |         |         | XDP RXQ1|         |
+---------+---------+---------+---------+---------+
| IRQ/TXQ | TX0 RX0 | TX2 RX2 | TX3 RX1 |         |
|         |         |         |         |         |
+---------+---------+---------+---------+---------+

TGL-H 1 port(1-way latency test) configuration:
+---------+---------+---------+---------+---------+
|  TGLH   |  CPU0   |  CPU1   |  CPU2   | CPU3    |
|         |         |         |         |         |
+=========+=========+=========+=========+=========+
| App     | iperf   | PTP     | TX/RX   |         |
|         | others  |         |         |         |
|         |         |         |         |         |
+---------+---------+---------+---------+---------+
| XDPQ    |         |         | XDP TXQ1|         |
|         |         |         | XDP RXQ1|         |
+---------+---------+---------+---------+---------+
| IRQ/TXQ | TX0 RX0 | TX2 RX2 | TX3 RX1 |         |
|         |         |         |         |         |
+---------+---------+---------+---------+---------+

TGL-H 2 port(2-way/return latency test) configuration:
+---------+---------+---------+---------+---------+
|  TGLH   |  CPU0   |  CPU1   |  CPU2   | CPU3    |
|         |         |         |         |         |
+=========+=========+=========+=========+=========+
| App     | iperf   | PTP     | TX      |  RX     |
|         | others  |         |         |         |
|         |         |         |         |         |
+---------+---------+---------+---------+---------+
| XDPQ    |         |         | XDP TXQ1| XDP RXQ1|
|         |         |         | (TxQ3)  | (RxQ1)  |
+---------+---------+---------+---------+---------+
| IRQ/TXQ | TX0 RX0 | TX1 RX2 | TX3     | RX3     |
|         |         |         |         |         |
+---------+---------+---------+---------+---------+

#Not shown: TAPRIO tx-queue & VLAN RX steering mapping
```

Additionally, task scheduler policy and task priorities can be changed to
further reduce/prevent un-scheduled interruptions. TXRX-TSN and OPCUA-SERVER
uses SCHED_FIFO.

#### Application-level

Finally, the task/workload itself should be sensible and properly designed.
Intervals should not be smaller than the time it takes to complete the task, or
too large to hog the CPU and cause the task to be paused for other deprived tasks.

Intervals and offsets should also be large enough to account for scheduling
jitter. Refer to: cyclictest.

### Role of run.sh

Given the previous section's list of considerations. It would be difficult to
configure each part of the project during each run. To avoid writing a
50-step readme, we script it under run.sh and corresponding config/json files.

Each example has a setup and run script for either Board A or Board B.

For specifics, see: [README_shell.md](README_shell.md) and [README_shell.md](README_json.md).

Here, we show how the flow of each runner (scripting wise) when a user enters a command
using run.sh.

#### Shell runner flow

```sh
#Setup only
run.sh <$PLAT> <$IFACE> [$IFACE2] $CONFIG setup
    |__ source ./shell/$PLAT/$CONFIG.config
    |__ init interface $IFACE
    |__ setup $CONFIG.sh

#Run only
run.sh <$PLAT> <$IFACE> [$IFACE2] $CONFIG run
    |__ source ./shell/$PLAT/$CONFIG.config
    |__ $CONFIG.sh
```

TXRX-TSN & TSQ take inputs from command line. These are passed into it using .config files.
in _shell/$PLAT/*.config_

Run.sh serves only as a shortcut to call the required scripts.

#### JSON runner flow

```sh
#Init
run.sh <$PLAT> <$IFACE> [$IFACE2] $CONFIG init
    |__ json/opcua-run.sh
        |__ init interface $IFACE ./json/$PLAT/opcua-*.config
#Setup
run.sh <$PLAT> <$IFACE> [$IFACE2] $CONFIG setup
    |__ json/opcua-run.sh ./json/$PLAT/$CONFIG-tsn.json.i
        |__ json/gen setup.py ./json/$PLAT/$CONFIG-tsn.json
            |__ gen iperf cmd
            |__ generated setup.sh
                |__ ptp4l
                |__ phc2sys
                |__ tc
                |__ iperf3 server
#Run
run.sh <$PLAT> <$IFACE> [$IFACE2] $CONFIG run
    |__ json/opcua-run.sh ./json/$PLAT/$CONFIG.json.i
        |__ ./iperf-gen-cmd.sh
        |__ ./opcua-server ./json/$PLAT/$CONFIG.json
        |__ gnuplot & save results
```

OPCUA-SERVER uses JSON files for input.

Run.sh serves only as a shortcut to call the required scripts and retrieve the right
JSON.i files. Each example has either a *.json.i or *-tsn.json.i file.

## File Structure & Conventions

Folder structure
* src - source files for all C code
* common - common configuration files, used by both shell & json runners.
* shell - shell specific configuration and scripts for tsq and txrx-tsn
* json - json specific configuration and scripts for opcua-server
* run.sh - primary point of user interaction

For each supported example, a platform-specific file is available for it.
If it doesn't exist means its not supported.

Platform folders with **2** at the back denotes 2-port configuration, meaning a total of 4 ports
are used between 2 platforms.

### Terminology


Timing terminology, for TXRX-TSN & OPCUA-SERVER
```
Base-time                               0
Cycle-time      -->|<------------------>|<------------------>|<------------------>|<---
                   |                    |                    |                    |
EarlyOffset        |            >-------|-<          >-------|-<          >-------|-<
PubOffset          |                    |-<                  |-<                  |-<
                   |                    |                    |                    |
SubOffset          |                    |--<                 |--<                 |--
                   |                    |                    |                    |
EarlyReturnOffset  |                    |   >------<         |   >------<         |
PubReturnOffset    |                    |----------<         |----------<         |--
                   |                    |                    |                    |
SubReturnOffset    |                    |------------<       |------------<       |--

---- time-span
```

Threads timing, for TXRX-TSN (TXa, RXb) & OPCUA-SERVER (TXa, RXb, TXc, RXb) when
timing offsets are taken into account.
```
Base-time                               0
Cycle-time      -->|<------------------>|<------------------>|<------------------>|<---
                   |                    |                    |                    |
TXa                |              ~~~~~ |              ~~~~~ |              ~~~~~ |
                   |                    |                    |                    |
RXb                |                    |  ---               |  ---               | -
                   |                    |                    |                    |
TXc                |                    |      -----         |      -----         |
                   |                    |                    |                    |
RXb                |                    |             ~~~~   |             ~~~~   |

~~~~ Thread running on Board A
---- Thread running on Board B
```

Terminology: example vs config
```
     BOARD A                                BOARD B
                                                    
     CONFIGa / JSONa                CONFIGb / JSONb
     SCRIPTSa                              SCRIPTSb
     APP                                        APP
    |______________________________________________|
                             |                      
                          EXAMPLE                   
```