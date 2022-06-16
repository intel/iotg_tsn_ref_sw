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

# Check OS distro
echo -e "\nINSTALL-DEPENDENCIES.SH: Checking OS Distro"
os_distro=$(cat /etc/os-release | grep -w NAME | cut -c 6-)
echo "OS Distro: $os_distro"


# Configure proxy
echo -e "\nINSTALL-DEPENDENCIES.SH: Configuring proxy"
echo -e export https_proxy=http://proxy.png.intel.com:911
echo -e git config --global https.proxy http://proxy.jf.intel.com:911
export https_proxy=http://proxy.png.intel.com:911
git config --global https.proxy http://proxy.jf.intel.com:911


# Check if if_xdp.h have XDP header
echo -e "\nINSTALL-DEPENDENCIES.SH: Checking XDP+TBS Availability"
check_xdp=$(cat /boot/config-$(uname -r)* | grep -i CONFIG_XDP_SOCKETS=y > /dev/null && echo 0 || echo 1)
txtime=$(cat /usr/include/linux/if_xdp.h | grep -i txtime > /dev/null && echo 0 || echo 1)
if [[ "$check_xdp" == "0" && "$txtime" == "0" ]]; then
    echo -e "Checking for XDP+TBS availability in kernel... yes"
else
    echo -e "Checking for XDP+TBS availability in kernel... no"
    echo -e "Currently TSN Ref Sw App only support on kernel that support XDP+TBS"
    exit 0
fi


# Add XDP header in if_xdp.h
#xdp_header=$(cat /usr/include/linux/if_xdp.h | grep -i xdp_desc > /dev/null && echo 0 || echo 1)
#txtime=$(cat /usr/include/linux/if_xdp.h | grep -i txtime > /dev/null && echo 0 || echo 1)
#if [[ "$xdp_header" == "0" && "$txtime" == "1" && "$check_xdp" == "0" ]]; then
#    sed -i '107i \\t__u64 txtime;' /usr/include/linux/if_xdp.h
#fi


# Local Variable Declaration
LOCAL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)


# Install libbpf
SRCREV_LIBBPF="d1fd50d475779f64805fdc28f912547b9e3dee8a"
echo -e "\n============================================="
echo -e "INSTALL-DEPENDENCIES.SH: Installing libbpf"
echo -e "============================================="
cd libbpf
rm -rf libbpf
git clone https://github.com/libbpf/libbpf.git
cd libbpf

echo -e "\nINSTALL-DEPENDENCIES.SH: Switch to branch: '$SRCREV_LIBBPF'"
git checkout $SRCREV_LIBBPF

echo -e "\nINSTALL-DEPENDENCIES.SH: Applying patches to libbpf"
EMAIL=root@localhost git am ../patches/*.patch

# Modify libbpf MakeFiles to build custom libbpf library
if [ "$os_distro" == "\"Ubuntu"\" ]; then
    sed -i 's/LIBDIR ?= $(PREFIX)\/$(LIBSUBDIR)/LIBDIR ?= $(PREFIX)\/lib\/x86_64-linux-gnu/g' src/Makefile
    sed -i 's/|@LIBDIR@|$(LIBDIR)/|@LIBDIR@|$(LIBDIR)\/libbpf-iotg-custom/g' src/Makefile
fi
sed -i 's/$(SHARED_LIBS),$(LIBDIR)/$(SHARED_LIBS),$(LIBDIR)\/libbpf-iotg-custom/g' src/Makefile
sed -i 's/\<bpf,644\>/bpf-iotg-custom,644/g' src/Makefile
sed -i 's/$(OBJDIR)\/libbpf.pc/$(OBJDIR)\/libbpf-iotg-custom.pc/g' src/Makefile
sed -i 's/Cflags: -I${includedir}/Cflags: -I${includedir}\/bpf-iotg-custom/g' src/libbpf.pc.template

echo -e "\nINSTALL-DEPENDENCIES.SH: Compiling libbpf"
NO_PKG_CONFIG=1 make -j$(nproc) -C src
DESTDIR=/ make install -j$(nproc) -C src
ldconfig


# Install libopen62541
SRCREV_LIBOPEN62541="a77b20ff940115266200d31d30d3290d6f2d57bd"
echo -e "\n============================================="
echo -e "INSTALL-DEPENDENCIES.SH: Installing open62541"
echo -e "============================================="
cd $LOCAL_DIR
cd open62541
rm -rf open62541
git clone https://github.com/open62541/open62541.git
cd open62541

echo -e "\nINSTALL-DEPENDENCIES.SH: Switch to branch: '$SRCREV_LIBOPEN62541'"
git checkout $SRCREV_LIBOPEN62541

echo -e "\nINSTALL-DEPENDENCIES.SH: Applying patches to open62541"
EMAIL=root@localhost git am ../patches/*.patch

# Edit open62541 Makefile to build custom open62541 library
sed -i 's/<bpf/<bpf-iotg-custom/g' plugins/ua_pubsub_ethernet_xdp.c
sed -i 's/open62541-iotg/open62541-iotg-custom/g' CMakeLists.txt
sed -i 's/OUTPUT_NAME "open62541-iotg-custom"/OUTPUT_NAME "open62541-iotg"/g' CMakeLists.txt
sed -i 's/OUTPUT_NAME open62541-iotg-custom/OUTPUT_NAME open62541-iotg/g' CMakeLists.txt
sed -i 's/LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/DESTINATION ${CMAKE_INSTALL_LIBDIR}\/open62541-iotg-custom\//g' CMakeLists.txt
sed -i 's/ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/DESTINATION ${CMAKE_INSTALL_LIBDIR}\/open62541-iotg-custom\//g' CMakeLists.txt
sed -i 's/libdir=${prefix}\/@CMAKE_INSTALL_LIBDIR@/libdir=${prefix}\/@CMAKE_INSTALL_LIBDIR@\/open62541-iotg-custom/g' open62541.pc.in
sed -i 's/libdir=${prefix}\/@CMAKE_INSTALL_INCLUDEDIR@/libdir=${prefix}\/@CMAKE_INSTALL_INCLUDEDIR@\/open62541-iotg-custom/g' open62541.pc.in
sed -i 's/Cflags: -I${includedir}\/open62541-iotg/Cflags: -I${includedir}\/open62541-iotg-custom/g' open62541.pc.in

echo -e "\nINSTALL-DEPENDENCIES.SH: Compiling open62541"
mkdir build
cd build
if [ "$check_xdp" == "1" ]; then
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


# Redirect to default directory
cd $LOCAL_DIR
cd ..

check_ld=$(cat /etc/environment | grep -i "LD_LIBRARY_PATH" > /dev/null && echo 0 || echo 1)
check_includedir=$(cat Makefile.am | grep -i "/usr/include/" > /dev/null && echo 0 || echo 1)
check_rpath=$(cat Makefile.am | grep -i "rpath" > /dev/null && echo 0 || echo 1)
check_txrx_afxdp_c=$(cat src/txrx-afxdp.c | grep -i "bpf-iotg-custom" > /dev/null && echo 0 || echo 1)
check_txrx_h=$(cat src/txrx.h | grep -i "bpf-iotg-custom" > /dev/null && echo 0 || echo 1)

# Set Dynamic Loader
if [ "$os_distro" == "\"Ubuntu"\" ]; then
    if [ "$check_ld" == "1" ]; then
        echo 'export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu/libbpf-iotg-custom:/usr/lib/x86_64-linux-gnu/open62541-iotg-custom:$LD_LIBRARY_PATH"' >> /etc/environment
    fi
else
    if [ "$check_rpath" == "1" ]; then
        sed -i 's/-Wl,-z/-Wl,--rpath=\/usr\/lib64\/libbpf-iotg-custom:\/usr\/lib64\/open62541-iotg-custom,-z/g' Makefile.am
    fi
fi
if [ "$check_includedir" == "1" ]; then
    sed -i '/-fno-common/a\\t\t-I/usr/include/bpf-iotg-custom -I/usr/include/open62541-iotg-custom \\' Makefile.am
fi
sed -i 's/\[libbpf\])/\[libbpf-iotg-custom\])/g' configure.ac
sed -i 's/\[open62541-iotg\])/\[open62541-iotg-custom\])/g' configure.ac

# Edit open62541 header file to redirect to custom library
if [ "$check_txrx_afxdp_c" == "1" ]; then
    sed -i 's/<bpf/<bpf-iotg-custom/g' src/txrx-afxdp.c
fi
if [ "$check_txrx_h" == "1" ]; then
    sed -i 's/<bpf/<bpf-iotg-custom/g' src/txrx.h
fi

# Use command 'source /etc/environment'
echo -e "Please Re-login or source /etc/environment for library to link up"

# Solution for git clone fail
# 1. Please configure the proxy according to your proxy setting before git clone
# 2. Please configure the system date up-to-date before git clone
# 3. Please reboot your system before git clone

exit 0
