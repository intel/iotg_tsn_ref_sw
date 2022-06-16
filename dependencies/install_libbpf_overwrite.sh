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

echo -e "\nINSTALL-DEPENDENCIES.SH: Compiling libbpf"
NO_PKG_CONFIG=1 make -j$(nproc) -C src
DESTDIR=/ make install -j$(nproc) -C src
ldconfig

exit 0
