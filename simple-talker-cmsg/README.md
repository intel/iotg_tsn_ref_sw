# License
The use of this Time Sensitive Networking (TSN) Reference Software is licensed
under the BSD 3-Clause License.

# Description
This application demonstrates the use of the Credit Based Shaper
(CBS, IEEE 802.1Qav) and the LaunchTime feature of Intel® Ethernet Controller
I210 to ensure bounded transmission latency for time sensitive and
loss-sensitive real-time data stream. In addition, this example demonstrates
the use of the Stream Reservation Protocol (SRP) as described in IEEE 802.1Qat
for time-sensitive traffic resource management.

For a more detailed description please refer to Section 4.0 of
[TSN Reference Software User Guide](../doc/README.md).

# Dependencies
## Hardware
- Intel® Ethernet Controller I210

## Software
- iperf3
- linuxptp
- OpenAvnu
- phc2sys
- tcpdump
- Wireshark application on Windows machine

# Setup Configuration
## Hardware
Refer to Section 4.1 of [TSN Reference Software User Guide](../doc/README.md).

## Software
For platform-specific Yocto Project BSP setup & pre-requisites, refer to
Section 2.1 of [TSN Reference Software User Guide](../doc/README.md).

For application-specific build steps, refer to Section 4.2 of
[TSN Reference Software User Guide](../doc/README.md).

For application-specific run steps, refer to Section 4.3 of
[TSN Reference Software User Guide](../doc/README.md).
