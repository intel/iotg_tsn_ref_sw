# JSON Runner

## JSON.i

Important: when using run.sh, users should modify the corresponding
**.json.i** file NOT the generated .json file. **.json.i** denotes incomplete or
interim json file as it has fields intentionally left blank for run.sh to fill in
automatically.

### Acceptable inputs and range for json.i

|     Category      |            Field             |  Type   |  Min   |   Max    |
| ----------------- | ---------------------------- | ------- | ------ | -------- |
| Mandatory/General | publisher_interface          | String  |        |          |
| Mandatory/General | subscriber_interface         | String  |        |          |
| Mandatory/General | use_xdp                      | Boolean |        |          |
| Mandatory/General | packet_count                 | Int     | 1      | 10000000 |
| Mandatory/General | cycle_time_ns                | Int     | 100000 | 5000000  |
| Mandatory/General | polling_duration_ns          | Int     | 0      | 10000000 |
| Publisher         | url                          | String  |        |          |
| Publisher         | pub_id                       | String  |        |          |
| Publisher         | dataset_writer_id            | Int     | 0      | 99999    |
| Publisher         | writer_group_id              | Int     | 0      | 99999    |
| Publisher         | early_offset_ns              | Int     | 0      | 1000000  |
| Publisher         | publish_offset_ns            | Int     | 0      | 10000000 |
| Publisher         | socket_prio                  | Int     | 0      | 7        |
| Publisher         | two_way_data                 | Boolean |        |          |
| Publisher         | cpu_affinity                 | Int     | 0      | 3        |
| Publisher         | xdp_queue                    | Int     | 1      | 3        |
| Subscriber        | url                          | String  |        |          |
| Subscriber        | sub_id                       | String  | 0      | 99999    |
| Subscriber        | subscribed_pub_id            | Int     | 0      | 99999    |
| Subscriber        | subscribed_dataset_writer_id | Int     | 0      | 99999    |
| Subscriber        | subscribed_writer_group_id   | Int     | 0      | 99999    |
| Subscriber        | offset_ns                    | Int     | 0      | 10000000 |
| Subscriber        | subscriber_output_file       | String  |        |          |
| Subscriber        | temp_file_dir                | String  |        |          |
| Subscriber        | two_way_data                 | Boolean |        |          |
| Subscriber        | cpu_affinity                 | Int     | 0      | 3        |
| Subscriber        | xdp_queue                    | Int     | 1      | 3        |

### Acceptable inputs and range for tsn-json.i

Refer to examples provided by the project.

## OPCUA-SERVER - AF_PACKET & AF_XDP OPCUA-based application

OPCUA-SERVER is a C-application that can transmit and receive ETH_UADP packets using
AF_PACKET or AF_XDP sockets using libopen62541 (OPCUA-based library) APIs. It
accepts only 1 .json.i file as input.


**NOTE:** The `PLAT` command line argument refers to either the CPU platform 
or the ethernet controller (e.g. the [i225](json/i225)), see in [json folder](json) 
the available platforms.

### About: 1-port

Very simple, just 1 pub/sub thread

At the end userspace-userpsace example, 1mil packet blabla

### Usage: 1-port

Similar to TSQ & TXRX-TSN examples, 2 platforms are required to be connected to
each other via a single-ethernet connection.

0.  [Board A & B] Build the project.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./build.sh
    ```

1.  [Board A] Run the setup script to initialize IP and MAC address, then start
    clock synchronization and setup TAPRIO + ETF qdiscs.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh $PLAT $IFACE opcua-pkt1a init
    ./run.sh $PLAT $IFACE opcua-pkt1a setup
    ```

2.  [Board B] Run the setup script to initialize IP and MAC address, then start
    clock synchronization and setup ingress qdisc.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh $PLAT $IFACE opcua-pkt1b init
    ./run.sh $PLAT $IFACE opcua-pkt1b setup
    ```

3.  [Board B] Start listening for packets.

    ```sh
    ./run.sh $PLAT $IFACE opcua-pkt1b run
    ```

4.  [Board A] Immediately after step 3, start transmitting packets. The
    application will terminate itself after completion. Note that AF_PACKET or
    opcua-pkt* configurations will take longer as they use much higher packet
    intervals compared to AF_XDP or opuca-xdp* configurations.

    ```sh
    ./run.sh $PLAT $IFACE opcua-pkt1a run
    ```

5.  [Board B] A gnuplot window should appear showing the time taken for each
    packet to traverse from the application in Board A to Board B.

    Each run will enumerate and store its raw timestamps/data & plot images in the
    results-<DATE> folder for easy reference, repeat steps 3-4 as needed.


### About: 2-port

In this example type, 2 platforms are required to be connected to each other via
**two** ethernet connections. Meaning there is a total of four
ethernet interfaces (as opposed to the usual two).

A simplified explanation of the data flow:
1. Board A's first interface ($IFACE1) is used to transmit
2. Board B's first interface ($IFACE1) is used to receive
3. Board B's second interface ($IFACE2) is used to transmit back to Board A
4. Board A's second interface ($IFACE2) is used to receive

### Usage: 2-port

When using dual-ports, the $PLAT should be replaced with "ehl2". (TGL not supported)

0.  [Board A & B] Build the project.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./build.sh
    ```

1.  [Board A] Run the setup script to initialize IP and MAC address, then start
    clock synchronization and setup TAPRIO + ETF qdiscs.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh $PLAT $IFACE $IFACE2 opcua-pkt3a init
    ./run.sh $PLAT $IFACE $IFACE2 opcua-pkt3a setup
    ```

2.  [Board B] Run the setup script to initialize IP and MAC address, then start
    clock synchronization and setup ingress qdisc.

    ```sh
    cd /usr/share/iotg-tsn-ref-sw/
    ./run.sh $PLAT $IFACE $IFACE2 opcua-pkt3b init
    ./run.sh $PLAT $IFACE $IFACE2 opcua-pkt3b setup
    ```

3.  [Board B] Start listening for packets.

    ```sh
    ./run.sh $PLAT $IFACE $IFACE2 opcua-pkt3b run
    ```

4.  [Board A] Immediately after step 3, start transmitting packets.

    ```sh
    ./run.sh $PLAT $IFACE opcua-pkt3a run
    ```

5.  [Board A] A gnuplot window should appear showing the time taken for each
    packet to traverse from the application in Board A to Board B and back to
    Board A.

    Each run will enumerate and store its raw timestamps/data & plot images in the
    results-<DATE> folder for easy reference, repeat steps 3-4 as needed.
