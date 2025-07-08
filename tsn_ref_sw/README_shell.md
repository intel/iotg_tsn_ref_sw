# Shell

## Config files

Config files are simply shell variable exports. Each config is typically divided
to 3 phases: init, setup, & runtime. These variables are checked against the script
and application input parsing.

## TSQ - Time Synchronization Quality Measurement

### About

The TSQ C-application retrieves externally-triggered hardware timestamps (by
PPS) and calculates the difference between the timestamps collected from two
platforms - which have their PTP clocks synchronized by PTP4L. To see what other
parameters can be used when calling TSQ, refer to: ./tsq --help

Note that TSQ is just a C-application and features such as AUXTS and PPS require
shell commands to enable/disable. setup-tsq1* & tsq1 scripts is used to perform
both the setting up and execution of the TSQ application.

### Usage

All examples are run with 2 units of the same platform. Mind the notation
"[Board A or B]". The following steps assumes both platforms are connected
to each other via an Ethernet connection and user has a terminal open in the
/usr/share/iotg-tsn-ref-sw directory - with the C-applications already built.

This example requires multiple pin header connections for (pulse-per-second)
PPS and (auxiliary timestamping) AUXTS.

**NOTE:** The `PLAT` command line argument refers to either the CPU platform 
or the ethernet controller (e.g. the [i225](shell/i225)), see in [shell folder](shell) 
the available platforms.

0.  [Board A & B] Build the project.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./build.sh
    ```

1.  [Board A] Run the setup script to configure IP and MAC address, start ptp4l
    , enable pulse-per-second and external timestamping.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh <PLAT> $IFACE tsq1a setup
    ```

2.  [Board B] Run the setup script to configure IP and MAC address, start ptp4l
    and enable external timestamping.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh <PLAT> $IFACE tsq1b setup
    ```

3.  [Board A] Start the talker and listener.

    ```sh
    ./run.sh <PLAT> $IFACE tsq1a run
    ```

4.  [Board B] Immediately after step 3, start the talker.

    ```sh
    ./run.sh <PLAT> $IFACE tsq1b run
    ```

5.  [Board A] A gnuplot window should appear indicating the difference between
    the external timestamps obtained from Board A & B. In an ideal environment,
    the difference should be 0 nanoseconds all the time. With ptp4l, the
    difference should be within the +/- 50 nanoseconds range. Both Board A &
    B's TSQ application will stop after 50 seconds.

## TXRX-TSN - AF_PACKET & AF_XDP socket-based application

### About

TXRX-TSN is a simple C-application that can transmit and receive packets using
AF_PACKET or AF_XDP sockets.

Specifically in this example, TXRX-TSN is used to transmit and receive packets using AF_PACKET
first, and then repeat using AF_XDP - to show the time taken for each packet to traverse from the application in Board A to Board B when IPERF3 is running in the background and TSN features enabled.

### Usage

All examples are run with 2 units of the same platform. Mind the notation
"[Board A or B]". The following steps assumes both platforms are connected
to each other via an Ethernet connection and user has a terminal open in the
/usr/share/iotg-tsn-ref-sw directory - with the C-applications already built.

0.  [Board A & B] Build the project.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./build.sh
    ```

1.  [Board A] Run the setup script to configure IP and MAC address, start clock
    synchronization and setup TAPRIO qdisc.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh <PLAT> $IFACE vs1a setup
    ```

2.  [Board B] Run the setup script to configure IP and MAC address, start clock
    synchronization and setup ingress qdiscs.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh <PLAT> $IFACE vs1b setup
    ```

3.  [Board B] Start listening for packets using AF_PACKET, it will automatically
    re-start the application using AF_XDP socket after it has completed. This
    duration will be printed on the shell line: "Phase xxx (N-duration seconds)"

    ```sh
    ./run.sh <PLAT> $IFACE vs1b run
    ```

4.  [Board A] Immediately after step 3, start transmitting packets using AF_PACKET.
    it will automatically re-start the application and trasnmit using AF_XDP
    socket after 30 seconds.

    ```sh
    ./run.sh <PLAT> $IFACE vs1a run
    ```

5.  [Board B] A gnuplot window should appear showing the time taken for each
    packet to traverse from the application in Board A to Board B. AF_XDP sockets
    should show an improvement in both overall average and jitter.

    Each run will enumerate and store its raw timestamps/data & plot images in the
    results-<DATE> folder for easy reference, repeat steps 3-4 as needed.
