# Configuration

## Config files

We have 2 types of configuration files. One is shell variable based and the other type is json-based *.json.i files.
These files are categorized according to the platform it supports.

The current default configuration is based on the current base settings based on highest SKU for each platform/silicons. User needs to change configs if the platform settings differ.

## Common change settings

1. gPTP config file

    We have different gPTP config for different PHY due to different ingress/egress latency values introduced by each PHY according to their operating speed.

    * Available files to be chosen is in "common" folder

        File convention : gPTP_<PHY_IDENTIFICATION>-<PHY speed>.cfg

        common
            ├── gPTP.cfg                            (generic gPTP)
            ├── gPTP_i225-1G.cfg                    (i225-based gPTP - 1G speed)
            ├── gPTP_RGMII-MV1510-1G.cfg            (Marvel 1510  - 1G speed)
            ├── gPTP_SGMII-MV2110-1G.cfg            (Marvel 2110  - 1G speed)
            ├── gPTP_SGMII-MV2110-2_5G.cfg          (Marvel 2110  - 2.5G speed)
            ├── gPTP_TI-1G.cfg                      (TI PHY  - 1G speed)

    * The default gPTP chosen for each platform depends on the main PHY POR for it.
    Default speed is also on 1G only.

    * The setting is stated in all the tests configuration:

        NOTE: Please change both sides (A and B) !! To ensure the gPTP can sync.

        TSQ ptp setting : shell/<platform>/tsq1X.config -> PTP_PHY_HW
        vs1 ptp setting : shell/<platform>/vs1X.config -> PTP_PHY_HW
        opcua-based test settings : json/<platform>/opcua-pktYX-tsn.json.i -> gPTP_file

2. Interrupt-to-core mapping

    TSN ref sw normally needs 3 different queues for 3 different type of traffic.
    The interrupts for these queues are sent to 3 different cores for performance optimization.
    1 core to process best-effort traffic, 1 core for PTP traffic and 1 core for our test traffic (measurement packets).

    By default, in order to ensure optimal performance, highest SKU (with normally min 4 cores) is used. In the event where less cores are available (i.e 2 core), 2 queues interrupt are going to be sent to 1 core (in this case PTP and best effort traffic).

    **NOTE: Performance is not guaranteed for less than 3 cores used.**

    The configuration mapping of these interrupts are in the "common" folder.

        File name convention : irq_affinity_<Number of core>-<Queue type and number>.cfg
            common
            ├── irq_affinity_2c_4tx_4rx.map         (2 core mapping for stmmac - 4 tx 4 rx q)
            ├── irq_affinity_2c_4TxRx.map           (2 core mapping for IGC/i225 - 4 TxRx q)
            ├── irq_affinity_4c_4tx_4rx_2way.map    (4 core mapping for stmmac - 4 tx 4 rx q(return))
            ├── irq_affinity_4c_4tx_4rx.map         (4 core mapping for stmmac - 4 tx 4 rx q)
            ├── irq_affinity_4c_4TxRx.map           (4 core mapping for IGC/i225 - 4 TxRx q)
            ├── irq_affinity_4c_8tx_8rx.map         (4 core mapping for stmmac - 8 tx 8 rx q)

    **How to read the mapping file**

    <Interrupt>,<core bitmask>,<comment(optional)>

    Example:

    tx-0,01,general -- tx-0(Interrupt for tx q 0) sends to core 0 (for general traffic)
    rx-2,02,ptp-rx  -- rx-2(Interrupt for rx q 2) sends to core 1 (ptp traffic)

    * The setting field for interrupt mapping in configuration files:

    TSQ ptp setting : shell/<platform>/tsq1X.config -> IRQ_AFFINITY_FILE
    vs1 ptp setting : shell/<platform>/vs1X.config -> IRQ_AFFINITY_FILE
    opcua-based test settings : json/<platform>/opcua-X.config -> IRQ_AFFINITY_FILE

## Default common configuration for each supported platform

1-port operation tests : tsq, vs1, opcua-pkt1
2-port operation tests : tsq, vs1, opcua-pkt1, opcua-pkt2, opcua-pkt3. opcua-xdp2, opcua-xdp3

* TGL-U - 1-port operation, 4 cores silicon mapping, Marvel default phy (gPTP config)
* TGL-H - 2-port operation, 4 cores silicon mapping, Marvel default phy (gPTP config)
* EHL   - 2-port operation, 4 cores silicon mapping, Marvel default phy (gPTP config)
* ADL-S - 2-port operation, 4 cores silicon mapping, Marvel default phy (gPTP config)
* ADL-N - 1-port operation, 2 cores silicon mapping, TI PHY default phy (gPTP config)
* RPL-S - 2-port operation, 4 cores silicon mapping, Marvel default phy (gPTP config)
* RPL-P - 1-port operation, 4 cores silicon mapping, Marvel default phy (gPTP config)
