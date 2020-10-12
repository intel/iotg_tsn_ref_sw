# Run an example

All examples are run with 2 units of the same platform. Mind the notation
"[Board A or B]". The following steps assumes both platforms are connected
to each other via an Ethernet connection and user has a terminal open in the
/usr/share/iotg-tsn-ref-sw directory - with the C-applications already built.

## Role of run.sh

Run.sh serves only as a shortcut to call the desired scripts.
The following section shows how to use it to run TSQ and TXRX-TSN examples.

Each example has a corresponding setup and run script for either
Board A or Board B. Curious users can review these scripts on what commands
are actually being run.

## TSQ - Time Synchronization Quality Measurement

Refer to the full documentation for hardware setup as this example requires
multiple pin header connections for (pulse-per-second) PPS and (auxiliary
timestamping) AUXTS.

The TSQ C-application retrieves externally-triggered hardware timestamps (by
PPS) and calculates the difference between the timestamps collected from two
platforms - which have their PTP clocks synchronized by PTP4L. To see what other
parameters can be used when calling TSQ, refer to: ./tsq --help

Note that TSQ is just a C-application and features such as AUXTS and PPS require
shell commands to enable/disable. setup-tsq1* & tsq1 scripts is used to perform
both the setting up and execution of the TSQ application.

1.  [Board A] Run the setup script to configure IP and MAC address, start ptp4l
    , enable pulse-per-second and external timestamping.

    ```bash
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh $IFACE setup-tsq1a
    ```

2.  [Board B] Run the setup script to configure IP and MAC address, start ptp4l
    and enable external timestamping.

    ```bash
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh $IFACE setup-tsq1b
    ```

3.  [Board A] Start the talker and listener.

    ```bash
    ./run.sh $IFACE tsq1a
    ```

4.  [Board B] Immediately after step 3, start the talker.

    ```bash
    ./run.sh $IFACE tsq1b
    ```

5.  [Board A] A gnuplot window should appear indicating the difference between
    the external timestamps obtained from Board A & B. In an ideal environment,
    the difference should be 0 nanoseconds all the time. With ptp4l, the
    difference should be within the +/- 50 nanoseconds range. Both Board A &
    B's TSQ application will stop after 50 seconds.

## TXRX-TSN - AF_PACKET & AF_XDP socket-based application

Refer to the full documentation for details as this README will serve as an
overview only.

TXRX-TSN is a simple C-application that can transmit and receive packets using
AF_PACKET or AF_XDP sockets. To see what parameters can be used when calling TXRX-TSN, refer to: ./txrx-tsn --help

Note that TXRX-TSN is just a C-application and setup-vs1* & vs1* scripts are
used in this example, to perform both the setting up and execution of TXRX-TSN.

Specifically in this example, TXRX-TSN is used to transmit and receive packets using AF_PACKET
first, and then repeat using AF_XDP - to show the time taken for each packet to traverse from the
application in Board A to Board B when IPERF3 is running in the background and TSN features enabled.

1.  [Board A] Run the setup script to configure IP and MAC address, start clock
    synchronization and setup TAPRIO qdisc.

    ```bash
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh $IFACE setup-vs1a
    ```

2.  [Board B] Run the setup script to configure IP and MAC address, start clock
    synchronization and setup ingress qdiscs.

    ```bash
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh $IFACE setup-vs1b
    ```

3.  [Board B] Start listening for packets using AF_PACKET, it will automatically
    re-start the application using AF_XDP socket after it has completed. This
    duration will be printed on the shell line: "Phase xxx (N-duration seconds)"

    ```bash
    ./run.sh $IFACE vs1b
    ```

4.  [Board A] Immediately after step 3, start transmitting packets using AF_PACKET.
    it will automatically re-start the application and trasnmit using AF_XDP
    socket after 30 seconds.

    ```bash
    ./run.sh $IFACE vs1a
    ```

5.  [Board B] A gnuplot window should appear showing the time taken for each
    packet to traverse from the application in Board A to Board B. AF_XDP sockets
    should show an improvement in both overall average and jitter.

    Each run will enumerate and store its raw timestamps/data & plot images in the
    results-<DATE> folder for easy reference, repeat steps 3-4 as needed.
