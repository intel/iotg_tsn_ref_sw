# IOTG TSN Reference Software for Linux

## Overview

This project is a set of C-applications _and_ scripts used to showcase different
__Ethernet-TSN__ features in Linux on specific Intel IOTG platform/software.

These serve as a practical example for those interested in developing TSN-capable
software.

This project introduces **three applications**, each with their own set of _examples_:

* **TSQ** - Time Synchronization Quality Measurement
  * _tsq1_ showcases the nanosecond-precision time synchronization between platforms
    using pulse-per-second (PPS) output & auxiliary timestamping (AUX_TS) input pins.

* **TXRX-TSN** - AF_PACKET & AF_XDP socket-based application
  * _vs1_ showcases the bounded low-latency transmission/reception achievable via AF_PACKET
    & AF_XDP standard linux socket interfaces, while leveraging various
    device-specific Ethernet-TSN features.

* **OPCUA-SERVER** - AF_PACKET & AF_XDP OPCUA-based application
  * _opcua-pkt1_ showcases the bounded low-latency
    transmission/reception achievable via AF_PACKET over libopen62541
    (an OPC-UA-based library) APIs, while leveraging various device-specific
    Ethernet-TSN features over a single-ethernet connection. This application
    uses JSON files as an input.
  * _opcua-pkt2_ &  _opcua-xdp2_ showcases the bounded low-latency
    transmission/reception achievable via AF_PACKET & AF_XDP over libopen62541
    (an OPC-UA-based library) APIs, while leveraging various device-specific
    Ethernet-TSN features over **two** ethernet connection. This application
    uses JSON files as an input and require platforms with at least 2
    Ethernet-TSN ports (e.g. EHL).
  * _opcua-pkt3_ & _opcua-xdp3_ is the same as _opcua-*2_ but with iperf3 background
    traffic.

## Compatibility

Currently supported hardwares are:

* Intel Atom XXX _(formerly Elkhart Lake) with its integrated Ethernet controller_
* Intel Core XXX _(formerly Tiger Lake UP3) with its integrated Ethernet controller_

This project is optimized to run on the supported hardware list and their
respective bundled softwares (IFWI, BSP, preempt-rt kernel) which has been
optimized for each platform's capabilities.

### Dependencies

List of dependencies in [DEPENDENCIES.md](DEPENDENCIES.md)

## Build

To build tsn ref sw, we are currently providing a single script that will build all
binaries (tsq, txrx-tsn and opcua-server).

    ```sh
    cd <tsn_ref_sw_directory>
    ./build.sh
    ```

## Documentation

### Contents

* [Scope](README_project.md#scope)
* [Objective](README_project.md#objective)
* [Design](README_project.md#design)
  * [Platform specificity](README_project.md#platform-specificity)
  * [Optimization](README_project.md#optimization)
  * [Role of run.sh](README_project.md#role-of-runsh)
* [File Structure & Conventions](README_project.md#file-structure-conventions)
* [Shell-based runner](README_shell.md)
  * [Config](README_shell.md#Config)
  * [TSQ](README_shell.md#tsq-time-synchronization-quality-measurement)
      * [About](README_shell.md#about)
      * [Usage](README_shell.md#usage)
  * [TXRX_TSN](README_shell.md#txrx-tsn-af_packet-af_xdp-socket-based-application)
      * [About](README_shell.md#about-1)
      * [Usage](README_shell.md#usage-1)
* [JSON-based runner](README_json.md)
  * [JSON.i](README_json.md#jsoni)
  * [OPCUA-SERVER](README_json.md#opcua-server-af_packet-af_xdp-opcua-based-application)
      * [About: 1-port](README_json.md#about-1-port)
      * [Usage: 1-port](README_json.md#usage-1-port)
      * [About: 2-port](README_json.md#about-2-port)
      * [Usage: 2-port](README_json.md#usage-2-port)
* [FAQ](README_faq.md)

### Disclaimer

* This project only serves to demonstrate TSN functionality and its
  usage on supported platforms and their environments.

* This project is not for intended for production use.

* This project is intended to be used with specific platforms and bsp, other
  hardware/software combinations YMMV

* Users are responsible for their own products' functionality and performance.

## Report Issues

If you see an issue, include these details in your issue submission:

* Hardware setup (Platform, Ethernet controller/NIC)
* Dependency version, refer to [DEPENDENCIES.md](DEPENDENCIES.md)
* OS or Linux distribution
* Linux kernel version
* Problem statement
* Steps to reproduce
* Images/Screenshots

## Contribute

Refer to [CONTRIBUTING.md](CONTRIBUTING.md)

## License

Refer to [LICENSE.md](LICENSE.md)

## FAQ

For tips on how to run tsn ref sw, please refer to [README_faq.md](README_faq.md)
It contains example of certain frequently seen run-time error.
