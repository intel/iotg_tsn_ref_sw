# IOTG TSN Reference Software for Linux

## Overview

This project is a set of scripts and C-applications used to showcase specific
__Ethernet-TSN__ features and workloads. Its many components can be used individually
in a variety of ways but this project will use them specifically for the
below 3 applications and their dedicated/respective examples (aka configs):

* **TSQ** [Time Synchronization Quality Measurement]
  * __tsq1__ showcases the nanosecond-precision time synchronization between platforms
    using pulse-per-second(PPS) output & auxiliary timestamping(AUX_TS) input pins.

* **TXRX-TSN** [AF_PACKET & AF_XDP socket-based application]
  * __vs1__ showcases the bounded low-latency transmission/reception achievable via AF_PACKET
    & AF_XDP standard linux socket interfaces, while leveraging various
    device-specific Ethernet-TSN features.

* **OPCUA-SERVER** [AF_PACKET & AF_XDP OPCUA-based application]
  * __opcua-pkt1__ showcases the bounded low-latency
    transmission/reception achievable via AF_PACKET over libopen62541
    (an OPC-UA-based library) APIs, while leveraging various device-specific
    Ethernet-TSN features over a single-ethernet connection. This application
    uses JSON files as an input.
  * __opcua-pkt2__, __opcua-pkt3__, __opcua-xdp2__ & __opcua-xdp3__ showcases
    the bounded low-latency transmission/reception achievable via AF_PACKET &
    AF_XDP over libopen62541 (an OPC-UA-based library) APIs, while leveraging
    various device-specific Ethernet-TSN features over **two** ethernet
    connection.This application uses JSON files as an input and require platforms
    with at least 2 Ethernet-TSN ports (e.g. EHL). Users can choose either __opcua-*2__
    or __opcua-*3__ to enable/disable background (iperf3) traffic.

Refer to the full documentation for details as this README serves as a
high-level overview only. Many specifics will not be present here due to the
project's complexity.

## Disclaimers

This project serves as an example to demonstrate TSN functionality only on the
supported platforms. This project is not for intended for production use.

Users are responsible for their own products functionality and performance.
Refer to LICENSE.txt for the license used by this project.

Each example defaults to a set of parameter values that has been tested on the
supported platforms to achieve ideal bounded-latency for each packet while the
platform is running on a preempt-rt kernel.

## Dependencies

This project is designed to work on specific Intel hardware and their
respective bundled software (e.g. IFWI, BSP, preempt-rt kernel). These examples
may not perform as expected when run on other platforms or configurations.
Currently supported platforms are:

* Elkhart Lake
* Tiger Lake UP3

These packages are required to be installed for the scripts to run. (Note: If
you are running from an Intel-provided Yocto BSP, these dependencies would have
already been installed):

* linuxptp - clock synchronization utilties (e.g. ptp4l & phc2sys)
* gnuplot
* python (3.7.7)
* libopen62541
* libbpf
* libelf
* librt
* libpthread
* libjson
* iperf3

### IceWM shortcuts:

A GUI/window manager should be used while executing these applications. The Yocto BSP
uses IceWM as its default window manager. Here are some keyboard shortcuts,
in case a mouse is not available.

Notation: C is control, M is meta/alt, S is shift, s is super

1. `<C-M-t>` to open xterm
2. `<M-f8>` to resize window (using arrow keys)
3. `<M-S-f10>` to maximise vertical space
4. `<M-f7>` to move window
5. `<C-M-leftarrow>` to move to the left, rightarrow for right
6. `<C-M-esc>` to show window list
7. `<M-space>` to show window actions menu

## Build the project

In a Yocto BSP with IOTG TSN Ref Sw included:

1. To build all 3 applications:

```bash
cd /usr/share/iotg-tsn-ref-sw/
./build.sh
```

## Run the examples

For instructions to run TSQ or TXRX-TSN,refer to [README_sh.md](README_sh.md)

For instructions to run OPCUA-SERVER, refer to [README_json.md](README_json.md)
