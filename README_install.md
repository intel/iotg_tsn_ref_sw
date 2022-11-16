# Installation HOWTO

## Tsn ref sw build and installation

To build tsn ref sw, we are currently providing a single script that will build all
binaries (tsq, txrx-tsn and opcua-server).

    ```sh
    cd <tsn_ref_sw_directory>
    ./build.sh <-h|-v|-V|-x|-t>
    ```

    **Build option**

    ```/usr/share/iotg_tsn_ref_sw# ./build.sh -h

        BUILD.SH: Checking Default Shell
        Default Shell: sh (Valid)
        Usage: ./build.sh [-hxtVv]

        -h,     -help,             --help             Display help

        -t,     -disablexdptbs,    --disablexdptbs    Disable Intel specific XDP+TBS support. (By default XDP+TBS support is enabled)

        -V,     -verbose,          --verbose          Run script in verbose mode.

        -v,     -version,          --version          Show version.
    ```

Default build (NO build options passed) is enabling XDP feature and Intel-specific XDP+TBS feature (if supported (if_xdp.h check)).

### To explicitly disable Intel specific XDP+TBS support in tsn ref sw

    ```sh
    cd <tsn_ref_sw_directory>
    ./build.sh -t
    ```

### Check version

    ```sh
    cd <tsn_ref_sw_directory>
    ./build.sh -v
    ```

NOTE

1. If if_xdp.h is not there, the XDP (and XDP_TBS as well) is considered as unsupported.
   In this case, opcua-server will not be compiled.
2. Table of current supported application according to if_xdp.h availability on the system.
   if_xdp.h indicates that the XDP socket is available to the applications side.
   Without if_xdp.h, only AF_PACKET-socket-based (tsq & txrx-tsn) test will be offered.

    |                                             |   tsq  | txrx-tsn  | opcua-server  |
    | ------------------------------------------- | ------ | --------- | ------------- |
    |WITHOUT XDP                                  |     Y  |     Y     |     N         |
    |- if_xdp.h not detected                      |        |           |               |
    | ------------------------------------------- | ------ | --------- | ------------- |

2. Table of current supported application according to xdp_tbs availability on the system.

    |                                             |    tsq | txrx-tsn  | opcua-server  |
    |---------------------------------------------|--------|-----------|---------------|
    |WITH XDP_TBS                                 |     Y  |     Y     |     Y         |
    |- if_xdp.h detected (WITH_XDP)               |        |           |               |
    |- XDP_TBS mode not disabled                  |        |           |               |
    |- txtime var is detected in if_xdp.h         |        |           |               |
    |---------------------------------------------|--------|-----------|---------------|
    |WITHOUT XDP_TBS                              |      Y |       Y   |     Y         |
    |- if_xdp.h detected (WITH_XDP)               |        |           |               |
    |- XDP_TBS mode disabled (-t) or              |        |           |               |
    |- txtime var is NOT detected in if_xdp.h     |        |           |               |
    |---------------------------------------------|--------|-----------|---------------|

Default build (NO -t) is enabling XDP feature and Intel-specific XDP+TBS feature (if XDP+TBS is supported in the system!)

Note: Effort is ongoing to decouple libopen62541-iotg fork and opcua-server from XDP_TBS Intel implementation.

Note: Performance of OPC UA related tests are not guarantee without Intel specific XDP+TBS support in tsn ref sw.

## Tsn sw ref dependencies installation

Refer to [Tsn Ref Sw Dependencies](DEPENDENCIES.md)
