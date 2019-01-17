# License
The use of this Time Sensitive Networking (TSN) Reference Software is licensed
under the BSD 3-Clause License

# Description
This application demonstrates time synchronization measurement quality between
the grandmaster clock and the slave clock by means of the gPTP (OpenAvnu) or
ptp4l (linuxptp) daemon. This involves the use of PTP clocks and the Ethernet
frame receive and transmit timestamping capability of the Intel® Ethernet
Controller I210. The time-stamping of the PTP clock by the auxilary timestamp
feature in Ethernet controller is triggered by 1PPS.

For a more detailed description please refer to Section 3.0 of
[TSN Reference Software User Guide](../doc/README.md).

# Dependencies
## Hardware
- Intel® Ethernet Controller I210

## Software
- daemon_cl
- gnuplot
- linuxptp
- open62541

# Setup Configuration
## Hardware
Refer to Section 3.1 of [TSN Reference Software User Guide](../doc/README.md).

## Software
For platform-specific Yocto Project BSP setup & pre-requisites, refer to
Section 2.1 of [TSN Reference Software User Guide](../doc/README.md).

For application-specific build steps, refer to Section 3.2 of
[TSN Reference Software User Guide](../doc/README.md).

For application-specific run steps, refer to Section 3.3 of
[TSN Reference Software User Guide](../doc/README.md).
