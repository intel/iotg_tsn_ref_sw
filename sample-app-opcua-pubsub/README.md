# License
The use of this Time Sensitive Networking (TSN) Reference Software is licensed
under the BSD 3-Clause License.

# Description
This application demonstrates the benefits of using Time-Aware Traffic Scheduling
and LaunchTime to reduce transmission jitter for scheduled traffic as measured
by inter-packet latency. Inter-packet latency measures the time between packets
as they are being transmitted, it reflects how deterministic the scheduled
traffic is being transmitted (within the defined cycle time). It leverages an
OPC UA stacks implementation under open-source project Open62541, to
demonstrate PubSub communication over Ethernet.

For a more detailed description please refer to Section 5.0 & Section 5.5.2 of
[TSN Reference Software User Guide](../doc/README.md).

# Dependencies
## Hardware
- Intel® Ethernet Controller I210

## Software
- gnuplot
- iperf3
- linuxptp
- open62541

# Setup Configuration
## Hardware
Refer to Section 5.1 of [TSN Reference Software User Guide](../doc/README.md).

## Software
For platform-specific Yocto Project BSP setup & pre-requisites, refer to
Section 2.1 of [TSN Reference Software User Guide](../doc/README.md).

For application-specific build steps, refer to Section 5.5.2 of
[TSN Reference Software User Guide](../doc/README.md).

For application-specific run steps, refer to Section 5.5.2 of
[TSN Reference Software User Guide](../doc/README.md).
