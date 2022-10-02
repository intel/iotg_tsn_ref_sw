# Software Dependencies

If you are running from a compatible Intel-provided Yocto BSP & hardware, these
software dependencies would have already been installed.

For compilation:
* [Custom linux kernel headers](https://github.com/intel/linux-intel-lts/tree/5.4/preempt-rt)
  - Include support for Intel XDP+TBS implementation
* [Custom linux-libc-headers](https://github.com/intel/iotg-yocto-ese-bsp/tree/master/recipes-kernel/linux-libc-headers/linux-libc-headers)
  - Include support for Intel XDP+TBS implementation
* [Custom libopen62541-iotg](https://github.com/intel/iotg-yocto-ese-main/tree/master/recipes-connectivity/open62541)
  - Include support for kernel implementation of Intel XDP+TBS (if_xdp.h change required)
* [Custom libbpf](https://github.com/intel/iotg-yocto-ese-main/tree/master/recipes-connectivity/libbpf)
  - Include support for kernel implementation of Intel XDP+TBS (if_xdp.h change required)
* libelf
* libjson-c

**NOTE**
Customized libbpf and libopen62541-iotg is a hard dependencies as per now (sorry). WIP to decouple tsn ref sw from the customized dependencies.
See below on how to install the customized libbpf and libopen62541-iotg.

For run-time:
* [Custom linux kernel](https://github.com/intel/linux-intel-lts/tree/5.4/preempt-rt)
* shell tools including awk/sed
* iproute2-ss200127
* linuxptp [v3.0](https://github.com/richardcochran/linuxptp/releases/tag/v3.0)
* Python 3.8.2
* gnuplot 5.2
* IceWM - Any GUI/window manager can be used, required to display graphs.

## Ubuntu based dependencies
In order to compile under Ubuntu, these packages need to be installed first:
* gcc
* autoconf
* libjson-c-dev
* gawk
* build-essential

## Dependencies installer script
In order to ease the installation of the customized helper libraries needed, we have included a few installation scripts in a specific branch.
These scripts will check out a specific version of the libraries from upstream git and apply our patches on top of it.

Branch name : **experimental/dependencies_installer**
The dependencies installer branch : https://github.com/intel/iotg_tsn_ref_sw/tree/experimental/dependencies_installer/

Refer to the README.md to understand the differences between the scripts provided:

https://github.com/intel/iotg_tsn_ref_sw/blob/experimental/dependencies_installer/dependencies/README.md

Suggestion is to use the overwriting installer script (after having kept the original libbpf and libopen62541 in a safe place).
This will ensure the tsn ref sw will be able to find/use correct libraries.

NOTE: If you are installing the libbpf and libopen62541-iotg manually, there is a chance the related open62541-iotg.pc file is not there. Hence, you might have to comment out this line in **configure.ac**.

**AM_CONDITIONAL([WITH_OPCUA], [test "x$no_open62541_iotg_fork" = "x0"] && test "x${HAVE_XDP}" = "xyes")**


# Hardware dependencies

* Refer to compatible platform list.

### IceWM

If you're using a compatible intel-developed Yocto BSP, IceWM is its default
window manager. Here are some keyboard shortcuts, in case a mouse is not available.

Notation: C is control, M is meta/alt, S is shift, s is super

1. `<C-M-t>` to open xterm
2. `<M-f8>` to resize window (using arrow keys)
3. `<M-S-f10>` to maximise vertical space
4. `<M-f7>` to move window
5. `<C-M-leftarrow>` to move to the left, `rightarrow` for right
6. `<C-M-esc>` to show window list
7. `<M-space>` to show window actions menu
