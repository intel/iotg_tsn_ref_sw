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

# Local Variable Declaration
LOCAL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

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

exit 0
