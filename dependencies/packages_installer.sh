#!/bin/bash
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

if [ $USER != "root" ]; then
    echo -e "Please run as root"
    exit 1
fi

# Local Variable Declaration
LOCAL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

main() {

# Check OS distro
check_distro

# Configure proxy
config_proxy

if [[ "$os_distro" == '"Ubuntu"' ]]; then
    echo -e "\nPACKAGES_INSTALLER.SH: Do you want to install generic packages?"
    while [[ ${generic_packages_installation^} != Y && ${generic_packages_installation^} != N ]];
    do
        echo -e "Enter [Y]es to proceed installation, [N]o to skip:"; read generic_packages_installation
    done

    if [[ ${generic_packages_installation^} == Y ]]; then
        #Check and install generic dependencies required
        install_generic_packages
        sleep 3
    else
        echo -e "PACKAGES_INSTALLER.SH: Skip generic packages installation."
    fi
fi

# Check whether the kernel support XDP_TBS
check_xdp_tbs

# Install libbpf
echo -e "\nPACKAGES_INSTALLER.SH: Do you want to install libbpf?"
while [[ ${libbpf_installation^} != Y && ${libbpf_installation^} != N ]];
do
    echo -e "Enter [Y]es to proceed installation, [N]o to skip:"; read libbpf_installation
done

if [[ ${libbpf_installation^} == Y ]]; then
    install_libbpf
    libbpf_path=$(ldconfig -p | grep -i libbpf.so.0 | head -n1 | cut -d ">" -f 2 | cut -d " " -f 2)
    ln -sf libbpf.so.0.7.0 $libbpf_path
    sleep 3
else
    echo -e "PACKAGES_INSTALLER.SH: Skip libbpf installation."
fi


# Install open62541-iotg
echo -e "\nPACKAGES_INSTALLER.SH: Do you want to install libopen62541-iotg?"
while [[ ${open62541_iotg_installation^} != Y && ${open62541_iotg_installation^} != N ]];
do
    echo -e "Enter [Y]es to proceed installation, [N]o to skip:"; read open62541_iotg_installation
done

if [[ ${open62541_iotg_installation^} == Y ]]; then
    install_open62541_iotg
    open62541_iotg_path=$(ldconfig -p | grep -i libopen62541-iotg | head -n1 | cut -d ">" -f 2 | cut -d " " -f 2)
    ln -sf libopen62541-iotg.so.1.1.0 $open62541_iotg_path
else
    echo -e "PACKAGES_INSTALLER.SH: Skip libopen62541-iotg installation."
fi

}

# *****************************************************************************
# *****************************************************************************

check_distro(){

    # Check OS distro
    echo -e "\nPACKAGES_INSTALLER.SH: Checking OS Distro"
    os_distro=$(cat /etc/os-release | grep -w NAME | cut -c 6-)
    echo "OS Distro: $os_distro"

}

config_proxy(){

    # Configure proxy
    echo -e "\nPACKAGES_INSTALLER.SH: Configuring proxy"
    echo -e "export https_proxy=http://proxy.png.intel.com:911"
    echo -e "git config --global https.proxy http://proxy.jf.intel.com:911"
    export https_proxy=http://proxy.png.intel.com:911
    git config --global https.proxy http://proxy.jf.intel.com:911

}

check_xdp_tbs(){

    # Check whether the kernel support XDP_TBS
    echo -e "\nPACKAGES_INSTALLER.SH: Checking XDP+TBS Availability"

    if [[ -f /usr/include/linux/if_xdp.h ]]; then
        txtime=$(cat /usr/include/linux/if_xdp.h | grep -i txtime > /dev/null && echo yes || echo no)
    else
        echo -e "WARNING: Header file if_xdp.h is not found"
        echo -e "System proceed to exit the installation as if_xdp.h not found"
        exit 1
    fi

    if [[ -f /proc/config.gz ]]; then
        cat /proc/config.gz | gunzip > $LOCAL_DIR/kernel.config
        xdp_supported=$(cat $LOCAL_DIR/kernel.config | grep -i CONFIG_XDP_SOCKETS=y > /dev/null && echo yes || echo no)
        if [[ "$xdp_supported" == "no" ]]; then
            echo -e "WARNING: CONFIG_XDP_SOCKETS is not enabled in /proc/config.gz in the kernel."
            echo -e "Installation will proceed as if_xdp.h exist."
            echo -e "Error/Failure might occur on XDP related test if running kernel is not having XDP socket support compiled in"
        fi
    elif [[ -f /boot/config-$(uname -r) ]]; then
        xdp_supported=$(cat /boot/config-$(uname -r)* | grep -i CONFIG_XDP_SOCKETS=y > /dev/null && echo yes || echo no)
        if [[ "$xdp_supported" == "no" ]]; then
            echo -e "WARNING: CONFIG_XDP_SOCKETS is not enabled in /boot/config-$(uname -r) in the kernel."
            echo -e "Installation will proceed as if_xdp.h exist."
            echo -e "Error/Failure might occur on XDP related test if running kernel is not having XDP socket support compiled in"
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

install_generic_packages() {

    echo -e "\n============================================================================"
    echo -e "PACKAGES_INSTALLER.SH: Installing Generic Ubuntu Dependencies for TSN Ref Sw"
    echo -e "============================================================================"

    # Check for gawk
    package="gawk"
    package_installation

    # Check for autoconf
    package="autoconf"
    package_installation

    # Check for Build-Essential
    package="build-essential"
    package_installation

    # Check for GCC
    package="gcc"
    package_installation

    # Check for Gnuplot
    package="gnuplot"
    package_installation

    # Check for Cmake
    package="cmake"
    package_installation

    # Check for Pkg-config
    package="pkg-config"
    package_installation

    # Check for Zlib1g
    package="zlib1g"
    package_installation

    # Check for Zlib1g-dev
    package="zlib1g-dev"
    package_installation

    # Check for Libelf-dev
    package="libelf-dev"
    package_installation

    # Check for Libjson-c-dev
    package="libjson-c-dev"
    package_installation

    # Check for Xterm
    package="xterm"
    package_installation

    # Check for linuxptp
    package="linuxptp"
    package_installation

    # Check for iperf3
    package="iperf3"
    package_installation
}

package_installation() {

    echo -e "\nChecking for $package ..."
    package_check=$(dpkg-query --show $package | grep -E '[0-9]' > /dev/null && echo 0 || echo 1)

    if [[ $package_check == 1 ]]; then
        echo -e "PACKAGES_INSTALLER.SH: $package not found! Proceed to install $package ..."
        sudo apt-get install $package
    fi

    package_check=$(dpkg-query --show $package | grep -E '[0-9]' > /dev/null && echo 0 || echo 1)

    if [[ $package_check == 1 ]]; then
        echo -e "\nPACKAGES_INSTALLER.SH: Please check your system's proxy setting and update the system clock before retry."
        echo -e "\nWARNING: Package/Library failed to installed! Installation terminated!"
        exit 1
    fi

    package_ver=$(dpkg-query --show $package)

    echo -e "\n===== $package ====="
    echo -e "Package Version: $package_ver\n"

}

install_libbpf() {

    # Git clone libbpf
    echo -e "\n============================================="
    echo -e "PACKAGES_INSTALLER.SH: Installing libbpf"
    echo -e "============================================="
    cd libbpf
    rm -rf libbpf
    git clone https://github.com/libbpf/libbpf.git

    if [[ ! -d libbpf ]]; then
        echo -e "\nPACKAGES_INSTALLER.SH: Please check your system's proxy setting and update the system clock before retry."
        echo -e "WARNING: Libbpf failed to clone! Installation terminated!"
        exit 1
    fi

    cd libbpf

    echo -e "\nPACKAGES_INSTALLER.SH: Switching branch"
    git checkout tags/v0.7.0

    if [[ "$xdp_supported" == "yes" && "$txtime" == "yes" ]]; then
        echo -e "\nPACKAGES_INSTALLER.SH: Applying patches to libbpf"
        EMAIL=root@localhost git am ../patches/*.patch
    fi

    if [ "$os_distro" == '"Ubuntu"' ]; then
        sed -i 's/LIBDIR ?= $(PREFIX)\/$(LIBSUBDIR)/LIBDIR ?= $(PREFIX)\/lib\/x86_64-linux-gnu/g' src/Makefile
    fi

    echo -e "\nPACKAGES_INSTALLER.SH: Compiling libbpf"
    NO_PKG_CONFIG=1 make -j$(nproc) -C src
    DESTDIR=/ make install -j$(nproc) -C src
    ldconfig

    # Libbpf path
    libbpf_path=$(ldconfig -p | grep -i libbpf | head -n1)

    if [[ -z "$libbpf_path" ]]; then
        echo -e "\nWARNING: Libbpf failed to installed! Installation terminated!"
        exit 1
    fi

    echo -e "\nChecking for libbpf... yes"
    echo -e "\n===== libbpf ====="
    echo -e "Library Path: $libbpf_path\n"

}

install_open62541_iotg() {

    # Git clone libopen62541-iotg
    echo -e "\n============================================="
    echo -e "PACKAGES_INSTALLER.SH: Installing libopen62541-iotg"
    echo -e "============================================="
    cd $LOCAL_DIR
    cd open62541
    rm -rf open62541
    git clone https://github.com/open62541/open62541.git

    if [[ ! -d open62541 ]]; then
        echo -e "\nPACKAGES_INSTALLER.SH: Please check your system's proxy setting and update the system clock before retry."
        echo -e "WARNING: Libpen62541-iotg failed to clone! Installation terminated!"
        exit 1
    fi

    cd open62541

    echo -e "\nPACKAGES_INSTALLER.SH: Switching branch"
    git checkout a77b20ff940115266200d31d30d3290d6f2d57bd

    # Apply patches for open62541-iotg
    if [[ "$xdp_supported" == "yes" && "$txtime" == "yes" ]]; then
        echo -e "\nPACKAGES_INSTALLER.SH: Applying IOTG patches with XDP_TBS to libopen62541"
        EMAIL=root@localhost git am ../patches/patches_w_xdp_tbs/*.patch
    else
        echo -e "\nPACKAGES_INSTALLER.SH: Applying IOTG patches without XDP_TBS to libopen62541"
        EMAIL=root@localhost git am ../patches/patches_wo_xdp_tbs/*.patch
    fi

    # Compile open62541
    echo -e "\nPACKAGES_INSTALLER.SH: Compiling libopen62541-iotg"

    mkdir build
    cd build

    if [[ "$xdp_support" == "no" ]]; then
        # Without XDP
        echo -e "Compiling libopen62541-iotg without XDP...."
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
        echo -e "Compiling libopen62541-iotg with XDP...."
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
    sed -i '/C_FLAGS/ s/$/ -Wno-error=deprecated-declarations/' CMakeFiles/open62541-plugins.dir/flags.make

    make && make install
    ldconfig

    # Libopen62541-iotg path
    open62541_iotg_path=$(ldconfig -p | grep -i libopen62541-iotg | head -n1)

    if [[ -z "$open62541_iotg_path" ]]; then
    echo -e "\nWARNING: Libopen62541-iotg failed to installed! Installation terminated!"
    exit 1
    fi

    echo -e "\nChecking for libopen62541-iotg... yes"
    echo -e "\n===== libopen62541-iotg ====="
    echo -e "Library Path: $open62541_iotg_path\n"

}

# Call main function
main

echo -e "\nSetup Successful."
echo -e "Please Reboot/Re-login or source /etc/environment for libraries to link up"

# Solution for git clone fail
# 1. Please configure the proxy according to your proxy setting before git clone
# 2. Please configure the system date up-to-date before git clone
# 3. Please reboot your system before git clone

exit 0
