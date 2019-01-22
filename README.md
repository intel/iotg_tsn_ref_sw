# TSN Reference Software for Linux
Time-Sensitive Networking (TSN) Reference Software for Linux.

## License
The project is licensed under the BSD 3-Clause License. Refer to
[LICENSE.txt](LICENSE.txt) for more information.

## Overview
This project presents a collection of sample applications which aim to
demonstrate and educate users on using TSN interface available in Linux mainline.

+ IEEE 802.1AS: [sample-app-1](sample-app-1/README.md)
    * Time Synchronization Quality Measurement
    * 1PPS (Pulse Per Second) & PTP clock time-stamping

+ IEEE 802.1Qav: [simple-talker-cmsg](simple-talker-cmsg/README.md)
    * Sample application to demonstrate the use of the Stream Reservation
      Protocol (SRP) as described in IEEE 802.1Qat for time-sensitive traffic
      resource management.
    * IEEE 802.1Qav Credit Based Shaper (CBS)
    * LaunchTime feature of Intel® Ethernet Controller I210 with ETF qdisc to
      ensure TX packets are in correct chronological order.

+ IEEE 802.1Qbv: [sample-app-taprio](sample-app-taprio/README.md)
    * TAPRIO qidsc (Software implementation of IEEE 802.1Qbv Time-Aware Shaper)
    * LaunchTime feature of Intel® Ethernet Controller I210 with ETF qdisc to
      ensure TX packets are in correct chronological order.

+ OPC UA Publish-Subscribe: [sample-app-opcua-pubsub](sample-app-opcua-pubsub/README.md)
    * Derived from Open62541 tutorial_pubsub_publish example
    * TAPRIO qidsc (Software implementation of IEEE 802.1Qbv Time-Aware Shaper)
    * LaunchTime feature of Intel® Ethernet Controller I210 with ETF qdisc to
      ensure TX packets are in correct chronological order.

## Compatibility and Dependencies
These applications are developed using the Intel® Ethernet Controller I210 which
has a specific LaunchTime feature. Theoretically, any other Ethernet controller
with the similar capability (with proper Linux driver support) would be able to
run these applications.

These applications should be compiled on the Linux Kernel 4.20 or higher to
enable the use of newly-introduced qdiscs:
- [CBS](http://man7.org/linux/man-pages/man8/tc-cbs.8.html)
- [ETF](http://man7.org/linux/man-pages/man8/tc-etf.8.html)
- [TAPRIO](http://man7.org/linux/man-pages/man8/tc-taprio.8.html)

Apart from that, each application has a slightly different set of dependencies.
For example, some require specialized hardware. Please check the individual
READMEs for more information.

## Documentation
A comprehensive User Guide can be found in the [doc folder](doc/README.md).

## How to contribute
To create and submit a pull request:
- Create a GitHub account
- Fork iotg_tsn_ref_sw on your site/account
- Create a branch from the "development" branch
- Push changes to your account, NOT intel/iotg_tsn_ref_sw - this will fail
- Create a pull request for your branch

## Contact
Submit Github issue to ask question, submit a request or report a bug.
