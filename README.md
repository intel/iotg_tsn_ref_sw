# IOTG TSN Reference Software for Linux

## Overview

This project is a set of scripts and C-applications used to showcase specific
__Ethernet-TSN__ features and workloads.

The included scripts and code serves as a practical example for those who wants
to develop TSN-capable software on specific Intel IOTG platforms/software.

This project introduces 3 **applications** and their dedicated/respective _examples_:

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
  * _opcua-pkt2_, _opcua-pkt3_, _opcua-xdp2_ & _opcua-xdp3_ showcases
    the bounded low-latency transmission/reception achievable via AF_PACKET &
    AF_XDP over libopen62541 (an OPC-UA-based library) APIs, while leveraging
    various device-specific Ethernet-TSN features over **two** ethernet
    connection.This application uses JSON files as an input and require platforms
    with at least 2 Ethernet-TSN ports (e.g. EHL). Users can choose either _opcua-*2_
    or _opcua-*3_ to enable/disable background (iperf3) traffic.

## Compatibility

Currently supported hardwares are:

* Elkhart Lake & Integrated NIC
* Tiger Lake UP3 & Integrated NIC

This project is optimized to run on the supported hardware list and their
respective bundled softwares (IFWI, BSP, preempt-rt kernel) which has been
optimized for each platform's capabilities.

Experienced programmers might be able to run (parts of) this project on different
hardwares/configurations. Contributions are welcome to extend this project's
list of supported hardware, as long as the above configurations are not
negatively impacted.

## Dependencies

Refer to DEPENDENCIES.md

## Disclaimer

This project only serves to demonstrate TSN functionality and its
usage on supported platforms and their environments.

This project is not for intended for production use.

Users are responsible for their own products functionality and performance.

## Usage

### Build the project

To build all 3 applications, cd into the folder and run the build script:

```sh
cd /usr/share/iotg-tsn-ref-sw/
./build.sh
```

### Run an example

To run TSQ or TXRX-TSN examples, refer to [README_sh.md](README_sh.md)

To run OPCUA-SERVER examples, refer to [README_json.md](README_json.md)

## Documentation

Full documentation is still a work-in-progress.

## How to Contribute

Refer to [CONTRIBUTING.md](CONTRIBUTING.md)

## How to Report Issues

If you see an issue, report it to seek assistance. Some details that'll speed
things up:

- Hardware setup (Platform, Ethernet controller/NIC)
- OS or Linux distribution
- Linux kernel version
- Version of dependencies. See: [DEPENDENCIES.md](DEPENDENCIES.md)
- Problem statement
- Steps to reproduce
- Images/Screenshots

## License

Refer to LICENSE.md
