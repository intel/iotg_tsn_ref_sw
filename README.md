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
```
Currently supported hardwares are:

	* Intel Atom XXX - (formerly Elkhart Lake) with its integrated Ethernet controller
	* Intel Core XXX - (formerly Tiger Lake UP3) with its integrated Ethernet controller

Currently supported systems are:

	* Yocto
	* Ubuntu
```

## Important
```
# Execute all the scripts in "root user"/"super user" privilege mode.
# There are two method to execute all the scripts in "root user"/"super user" privilege mode.

Method 1:
# Enter into "root user"/"super user" privilege mode using command below:
	sudo su

Method 2:
# Execute all the scripts using command below:
	sudo ./<script_name>.sh
```

## Dependencies

### General

#### *For system clock:*
```
# Please ensure the system clock is set to current date before using the installer script to install the required dependencies.
# Otherwise the installation will failed.
# Set the system clock to current date using command below:
	date -s YY/MM/DD
```

#### *For system shell:*
```
# Bash are required as system shell in order to compile and run TSN Reference Software.
# Install bash using command below:
	sudo apt-get install bash
```

#### *Packages Installer:*
```
# All the TSN Ref Sw App required dependencies can be install by using the packages installer below:

IMPORTANCE: Please read and follow the instruction below before using the packages installer!

# In order to ease the installation of generic packages and IOTG customized helper libraries (libbpf-iotg and sopen62541-iotg) needed,
# we have provide a installation script.
# The script will check out a specific version of the libraries from upstream git and apply our patches on top of it.

# Suggestion is to use the installer script (after having kept the original libbpf and libopen62541 in a safe place).
# This will ensure the tsn ref sw will be able to find/use correct libraries.

NOTE: If your kernel support for Intel XDP+TBS, please ensure the '__u64 txtime' descriptor is available in struct xdp_desc() in /usr/include/linux/if_xdp.h as example below:

/* Rx/Tx descriptor */
struct xdp_desc {
        __u64 addr;
        __u32 len;
        __u32 options;
        __u64 txtime;
};

WARNING: You are able to install libbpf & open62541 without the 'txtime' descriptor but you might facing error/failure during the tests.

# The packages installer branch : https://github.com/intel/iotg_tsn_ref_sw
# Branch name : **master**
# Use the commands below to install the packages:

	cd dependencies
	./packages_installer.sh

# NOTE: The packages installer only support the overwrite installation.
# NOTE: Refer to the below

# NOTE: If you are installing the libbpf and libopen62541-iotg manually, there is a chance the related open62541-iotg.pc file is not there.
# Hence, you might have to comment out this line in **configure.ac**.

**AM_CONDITIONAL([WITH_OPCUA], [test "x$no_open62541_iotg_fork" = "x0"] && test "x${HAVE_XDP}" = "xyes")**

# Disclaimer
	* The packages installers only serves to install dependencies for TSN Ref Sw App.
	* This project is not for intended for production use.
	* This project is intended to be used with specific platforms and bsp, other HW/SW combinations YMMV
	* Users are responsible for their own products' functionality and performance.

# FAQ

	If git clone fail, try the solution below:
	* Please configure the proxy according to your proxy setting before git clone
	* Please configure the system date up-to-date before git clone
	* Please reboot your system before git clone
```

### Yocto

#### *For compilation:*
```
# If you are running from a compatible Intel-provided Yocto BSP & hardware, these
# software dependencies would have already been installed.

	* [Custom linux kernel headers](https://github.com/intel/linux-intel-lts/tree/5.*/preempt-rt)
  	- Include support for Intel XDP+TBS implementation
	* [Custom linux-libc-headers](https://github.com/intel/iotg-yocto-ese-bsp/tree/master/recipes-kernel/linux-libc-headers/linux-libc-headers)
  	- Include support for Intel XDP+TBS implementation
	* [Custom libopen62541-iotg](https://github.com/intel/iotg-yocto-ese-main/tree/master/recipes-connectivity/open62541)
  	- Include support for kernel implementation of Intel XDP+TBS (if_xdp.h change required)
	* [Custom libbpf](https://github.com/intel/iotg-yocto-ese-main/tree/master/recipes-connectivity/libbpf)
  	- Include support for kernel implementation of Intel XDP+TBS (if_xdp.h change required)
	* libelf
	* libjson-c
```

#### *For run-time:*
```
	* [Custom linux kernel](https://github.com/intel/linux-intel-lts/tree/5.*/preempt-rt)
	* shell tools including awk/sed
	* iproute2-ss200127
	* linuxptp [v3.0](https://github.com/richardcochran/linuxptp/releases/tag/v3.0)
	* Python 3.8.2
	* gnuplot 5.2
	* IceWM - Any GUI/window manager can be used, required to display graphs.
```

#### *IceWM:*
```
# If you're using a compatible intel-developed Yocto BSP, IceWM is its default
# window manager. Here are some keyboard shortcuts, in case a mouse is not available.

# Notation: C is control, M is meta/alt, S is shift, s is super

1. `<C-M-t>` to open xterm
2. `<M-f8>` to resize window (using arrow keys)
3. `<M-S-f10>` to maximise vertical space
4. `<M-f7>` to move window
5. `<C-M-leftarrow>` to move to the left, `rightarrow` for right
6. `<C-M-esc>` to show window list
7. `<M-space>` to show window actions menu
```

### Ubuntu

#### *Ubuntu based dependencies:*
```
# In order to compile under Ubuntu, these packages need to be installed first (using sudo apt-get install ....) :
	* autoconf
	* build-essential
	* cmake
	* gawk
	* gcc
	* gnuplot
	* pkg-config
	* zlib1g
	* zlib1g-dev
	* libelf-dev
	* libjson-c-dev
	* xterm
	* iperf3
	* linuxptp

NOTE: ensure your proxy settings are correct.
NOTE: All the packages can be install by using the packages_installer.sh above.
```

## Build
```
# To build tsn ref sw, we are currently providing a single script that will build all
# binaries (tsq, txrx-tsn and opcua-server).

    ```
    cd <tsn_ref_sw_directory>
    ./build.sh
    ```

# To explicitly disable Intel specific XDP+TBS support in tsn ref sw

    ```
    cd <tsn_ref_sw_directory>
    ./build.sh -t
    ```
```
For further configuration details, check out [README_config.md](README_config.md) and [ShellConfig](README_shell.md#Config)

## Run
* For the details on how to run tsn ref sw, please refer to [Shell-based runner](README_shell.md) and [JSON-based runner](README_json.md)
---

This project is optimized to run on the supported hardware list and their respective bundled softwares (IFWI, BSP, preempt-rt kernel)
which has been optimized for each platform's capabilities.

For details information please refer to the documentation below:

## Documentation

### Contents

* [Scope](README_project.md#scope)
* [Objective](README_project.md#objective)
* [Design](README_project.md#design)
  * [Platform specificity](README_project.md#platform-specificity)
  * [Optimization](README_project.md#optimization)
  * [Role of run.sh](README_project.md#role-of-runsh)
* [Installation](README_install.md)
* [File Structure & Conventions](README_project.md#file-structure-conventions)
* [Configuration](README_config.md)
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


## Disclaimer

* This project only serves to demonstrate TSN functionality and its
  usage on supported platforms and their environments.

* This project is not for intended for production use.

* This project is intended to be used with specific platforms and bsp, other HW/SW combinations YMMV

* Users are responsible for their own products' functionality and performance.

## Report Issues

If you see an issue, include these details in your issue submission:

* Hardware setup (Platform, Ethernet controller/NIC)
* Dependency version
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
