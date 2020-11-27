# Software Dependencies

If you are running from a compatible Intel-provided Yocto BSP & hardware, these
software dependencies would have already been installed.

For compilation:
* [Custom linux kernel headers](https://github.com/intel/linux-intel-lts/tree/5.4/preempt-rt)
* [Custom linux-libc-headers](https://github.com/intel/iotg-yocto-ese-bsp/tree/master/recipes-kernel/linux-libc-headers/linux-libc-headers)
* [Custom libopen62541-iotg](https://github.com/intel/iotg-yocto-ese-main/tree/master/recipes-connectivity/open62541)
* [Custom libbpf](https://github.com/intel/iotg-yocto-ese-main/tree/master/recipes-connectivity/libbpf)
* libelf
* libjson-c

For run-time:
* [Custom linux kernel](https://github.com/intel/linux-intel-lts/tree/5.4/preempt-rt)
* shell tools including awk/sed
* iproute2-ss200127
* linuxptp [v3.0](https://github.com/richardcochran/linuxptp/releases/tag/v3.0)
* Python 3.8.2
* gnuplot 5.2
* IceWM - Any GUI/window manager can be used, required to display graphs.

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
