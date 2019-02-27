# Software Dependencies
All applications in this project are tested with:

+ [Apollo Lake Yocto BSP - MR4rt-B-01](https://github.com/intel/iotg-yocto-bsp-public/tree/MR4rt-B-01)

which contains out-of-tree patches inside its meta-intel-tsn layer for the
following:

## Kernel
+ __IGB Driver__

    * __Required for__: sample-app-1

    * __Location__: BSP/meta-intel-tsn/recipes-kernel/linux/

    * __Description__: A patch is needed to correct the negative/positive offset
      that occurred when master and slave timestamp are in sync. Because of the
      default TX and RX latency offset in igb driver, the Time Synchronization
      Error is not evenly distributed around zero, i.e. unbalanced when time
      synchronization is at locked state. To adjust the unevenly distributed
      Time Synchronization Error, the TX and RX latency offsets are re-tuned per
      the hardware platform used. Users are encouraged to run Demo 1 and
      fine-tune according to its hardware configuration to achieve evenly
      distributed Time Synchronization Error.

## Library/Application

+ __AVnu/gptp__
    * __Required for__: simple-talker-cmsg

    * __Location__: BSP/meta-intel-tsn/recipes-connectivity/openavnu/

    * __Description__: igb_avb module has been patched with standard kernel API in
      to configure SDP to output PPS (https://github.com/AVnu/OpenAvnu/pull/503).
      As a result, the gPTP daemon have a means to use the Linux kernel ioctl API,
      PTP_PIN_SETFUNC and PTP_PEROUT_REQUEST, to start/stop PPS instead of
      programming I210 registers directly through libigb.

+ __Open62541__
    * __Required for__: sample-app-taprio & sample-app-opcua-pubsub

    * __Location__: BSP/meta-intel-tsn/recipes-connectivity/open62541/

    * __Description__: Open62541 library does not have support for SO_PRIORITY
      and SO_TXTIME. These patches extend the library's publisher APIs to use
      these Linux socket options to steer packets to a specific queue and
      specify their per-packet transmit time.

+ __Linuxptp__
    * __Required for__: sample-app-taprio & sample-app-opcua-pubsub

    * __Location__: BSP/meta-intel-tsn/recipes-connectivity/linuxptp/

    * __Description__: This patch adds the capability to assign socket priority
      (SO_PRIORITY) 7 to PTP packets. This is needed so that we can manipulate
      PTP packets to map to the hw queue of our choice via mqprio or taprio. For
      this project, hw queue 2 is seletected because queue 0 and 1 are reserved
      for packets with LaunchTime feature.
