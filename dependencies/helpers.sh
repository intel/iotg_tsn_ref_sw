#!/bin/sh
#/******************************************************************************
#  Copyright (c) 2020, Intel Corporation
#  All rights reserved.

#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:

#   1. Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.

#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.

#   3. Neither the name of the copyright holder nor the names of its
#      contributors may be used to endorse or promote products derived from
#      this software without specific prior written permission.

#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
# *****************************************************************************/

# Helper functions. This script executes nothing.

###############################################################################

# Local Variable Declartion
LOCAL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

check_distro(){
    # Check OS distro

    echo -e "\nINSTALL-DEPENDENCIES.SH: Checking OS Distro"
    os_distro=$(cat /etc/os-release | grep -w NAME | cut -c 6-)
    echo "OS Distro: $os_distro"
}

config_proxy(){
    # Configure proxy

    echo -e "\nINSTALL-DEPENDENCIES.SH: Configuring proxy"
    echo -e export https_proxy=http://proxy.png.intel.com:911
    echo -e git config --global https.proxy http://proxy.jf.intel.com:911
    export https_proxy=http://proxy.png.intel.com:911
    git config --global https.proxy http://proxy.jf.intel.com:911
}

check_xdp_tbs(){
    # Check whether the kernel support XDP_TBS

    echo -e "\nINSTALL-DEPENDENCIES.SH: Checking XDP+TBS Availability"

    if [[ -f /usr/include/linux/if_xdp.h ]]; then
        txtime=$(cat /usr/include/linux/if_xdp.h | grep -i txtime > /dev/null && echo yes || echo no)
    else
        echo -e "WARNING: Header file if_xdp.h is not found"
        echo -e "System proceed to exit the installation as if_xdp.h not found"
        exit 0
    fi

    if [[ -f /proc/config.gz ]]; then
        cat /proc/config.gz | gunzip > $LOCAL_DIR/kernel.config
        xdp_supported=$(cat $LOCAL_DIR/kernel.config | grep -i CONFIG_XDP_SOCKETS=y > /dev/null && echo yes || echo no)
        if [[ "$xdp_supported" == "no" ]]; then
            echo -e "WARNING: CONFIG_XDP_SOCKETS is not enabled in the kernel."
            echo -e "Installation will proceed as if_xdp.h exist. However it might crash when run the program"
        fi
    elif [[ -f /boot/config-$(uname -r) ]]; then
        xdp_supported=$(cat /boot/config-$(uname -r)* | grep -i CONFIG_XDP_SOCKETS=y > /dev/null && echo yes || echo no)
        if [[ "$xdp_supported" == "no" ]]; then
            echo -e "WARNING: CONFIG_XDP_SOCKETS is not enabled in the kernel."
            echo -e "Installation will proceed as if_xdp.h exist. However it might crash when run the program"
        fi
    else
        xdp_supported=no
        echo -e "WARNING: Kernel config is not found."
        echo -e "Installation will proceed as if_xdp.h exist. However it might crash when run the program"
    fi

    if [[ "$xdp_supported" == "yes" && "$txtime" == "yes" ]]; then
        echo -e "Checking for XDP+TBS availability in kernel... yes"
    else
        echo -e "Checking for XDP+TBS availability in kernel... no"
    fi
}

clone_libbpf(){
    # Git clone libbpf

    echo -e "\n============================================="
    echo -e "INSTALL-DEPENDENCIES.SH: Installing libbpf"
    echo -e "============================================="
    cd libbpf
    rm -rf libbpf
    git clone https://github.com/libbpf/libbpf.git
    cd libbpf

    echo -e "\nINSTALL-DEPENDENCIES.SH: Switching branch"
    git checkout d1fd50d475779f64805fdc28f912547b9e3dee8a

    if [[ "$xdp_supported" == "yes" && "$txtime" == "yes" ]]; then
        echo -e "\nINSTALL-DEPENDENCIES.SH: Applying patches to libbpf"
        EMAIL=root@localhost git am ../patches/*.patch
    fi
}

clone_open62541(){
    # Git clone libopen62541

    echo -e "\n============================================="
    echo -e "INSTALL-DEPENDENCIES.SH: Installing open62541"
    echo -e "============================================="
    cd $LOCAL_DIR
    cd open62541
    rm -rf open62541
    git clone https://github.com/open62541/open62541.git
    cd open62541

    echo -e "\nINSTALL-DEPENDENCIES.SH: Switching branch"
    git checkout a77b20ff940115266200d31d30d3290d6f2d57bd

    # Apply patches for open62541
    if [[ "$xdp_supported" == "yes" && "$txtime" == "yes" ]]; then
        echo -e "\nINSTALL-DEPENDENCIES.SH: Applying patches with XDP_TBS to open62541"
        EMAIL=root@localhost git am ../patches/patches_w_xdp_tbs/*.patch
    else
        echo -e "\nINSTALL-DEPENDENCIES.SH: Applying patches without XDP_TBS to open62541"
        EMAIL=root@localhost git am ../patches/patches_wo_xdp_tbs/*.patch
    fi
}

compile_open62541(){
    # Compile open62541

    echo -e "\nINSTALL-DEPENDENCIES.SH: Compiling open62541"

    mkdir build
    cd build

    if [ "$xdp_support" == "no" ]; then
        # Without XDP
        echo -e "Compiling open62541 without XDP...."
        cmake   -DUA_BUILD_EXAMPLES=OFF                             \
                -DUA_ENABLE_PUBSUB=ON                               \
                -DUA_ENABLE_PUBSUB_CUSTOM_PUBLISH_HANDLING=ON       \
                -DUA_ENABLE_PUBSUB_ETH_UADP=ON                      \
                -DUA_ENABLE_PUBSUB_ETH_UADP_ETF=ON                  \
                -DUA_ENABLE_PUBSUB_ETH_UADP_XDP=OFF                 \
                -DCMAKE_INSTALL_PREFIX=/usr                         \
                -DBUILD_SHARED_LIBS=ON ..
    else
        # With XDP
        echo -e "Compiling open62541 with XDP...."
        cmake   -DUA_BUILD_EXAMPLES=OFF                             \
                -DUA_ENABLE_PUBSUB=ON                               \
                -DUA_ENABLE_PUBSUB_CUSTOM_PUBLISH_HANDLING=ON       \
                -DUA_ENABLE_PUBSUB_ETH_UADP=ON                      \
                -DUA_ENABLE_PUBSUB_ETH_UADP_ETF=ON                  \
                -DUA_ENABLE_PUBSUB_ETH_UADP_XDP=ON                  \
                -DCMAKE_INSTALL_PREFIX=/usr                         \
                -DBUILD_SHARED_LIBS=ON ..
    fi

    # Modify CMakeFiles to ignore warnings
    sed -i '/C_FLAGS/ s/$/ -Wno-error=conversion/' CMakeFiles/open62541-plugins.dir/flags.make
    sed -i '/C_FLAGS/ s/$/ -Wno-error=maybe-uninitialized/' CMakeFiles/open62541-object.dir/flags.make

    make && make install
}
