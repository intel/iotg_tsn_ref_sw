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

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source $DIR/helpers.sh

# Local Variable Declaration
LOCAL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

# Check OS distro
check_distro

# Check whether the kernel support XDP_TBS
check_xdp_tbs

# Configure proxy
config_proxy

# Git clone libbpf
clone_libbpf

if [[ "$1" == "--overwrite" ]]; then
    echo -e "\nNOTE: The dependencies installer will overwrite the original libbpf"
    if [ "$os_distro" == "\"Ubuntu"\" ]; then
        sed -i 's/LIBDIR ?= $(PREFIX)\/$(LIBSUBDIR)/LIBDIR ?= $(PREFIX)\/lib\/x86_64-linux-gnu/g' src/Makefile
    fi
else
    # Modify libbpf MakeFiles to build custom libbpf library
    if [ "$os_distro" == "\"Ubuntu"\" ]; then
        sed -i 's/LIBDIR ?= $(PREFIX)\/$(LIBSUBDIR)/LIBDIR ?= $(PREFIX)\/lib\/x86_64-linux-gnu/g' src/Makefile
        sed -i 's/|@LIBDIR@|$(LIBDIR)/|@LIBDIR@|$(LIBDIR)\/libbpf-iotg-custom/g' src/Makefile
    fi
    sed -i 's/$(SHARED_LIBS),$(LIBDIR)/$(SHARED_LIBS),$(LIBDIR)\/libbpf-iotg-custom/g' src/Makefile
    sed -i 's/\<bpf,644\>/bpf-iotg-custom,644/g' src/Makefile
    sed -i 's/$(OBJDIR)\/libbpf.pc/$(OBJDIR)\/libbpf-iotg-custom.pc/g' src/Makefile
    sed -i 's/Cflags: -I${includedir}/Cflags: -I${includedir}\/bpf-iotg-custom/g' src/libbpf.pc.template
fi

echo -e "\nINSTALL-DEPENDENCIES.SH: Compiling libbpf"
NO_PKG_CONFIG=1 make -j$(nproc) -C src
DESTDIR=/ make install -j$(nproc) -C src
ldconfig

# Git clone libopen62541
clone_open62541

if [[ "$1" == "--overwrite" ]]; then
    echo -e "\nNOTE: The dependencies installer will overwrite the original open62541"
else
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
fi

# Compile open62541
compile_open62541

# Redirect to default directory
cd $LOCAL_DIR
cd ..

check_ld=$(cat /etc/environment | grep -i "LD_LIBRARY_PATH" > /dev/null && echo 0 || echo 1)
check_includedir=$(cat Makefile.am | grep -i "/usr/include/" > /dev/null && echo 0 || echo 1)
check_rpath=$(cat Makefile.am | grep -i "rpath" > /dev/null && echo 0 || echo 1)
check_txrx_afxdp_c=$(cat src/txrx-afxdp.c | grep -i "bpf-iotg-custom" > /dev/null && echo 0 || echo 1)
check_txrx_h=$(cat src/txrx.h | grep -i "bpf-iotg-custom" > /dev/null && echo 0 || echo 1)

# Link up the libraries
if [[ "$1" == "--overwrite" ]]; then
    echo -e "\nNOTE: The dependencies installer will link to the original libraries path"
else
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
fi

# Libraries path
if [[ "$1" == "--overwrite" ]]; then
    if [ "$os_distro" == "\"Ubuntu"\" ]; then
        libbpf_so_path=$(find /usr/* -maxdepth 2 -name libbpf.so.0)
        open62541_so_path=$(find /usr/* -maxdepth 2 -name libopen62541-iotg.so.1)
        libbpf_path=$(find /usr/* -maxdepth 2 -name libbpf.so.0.2.0)
        open62541_path=$(find /usr/* -maxdepth 2 -name libopen62541-iotg.so.1.1.0)
    else
        libbpf_so_path=$(find /usr/* -maxdepth 1 -name libbpf.so.0)
        open62541_so_path=$(find /usr/* -maxdepth 1 -name libopen62541-iotg.so.1)
        libbpf_path=$(find /usr/* -maxdepth 1 -name libbpf.so.0.2.0)
        open62541_path=$(find /usr/* -maxdepth 1 -name libopen62541-iotg.so.1.1.0)
    fi
else
    libbpf_path=$(find /usr/* -name libbpf.so.0.2.0 | grep -i libbpf-iotg-custom)
    open62541_path=$(find /usr/* -name libopen62541-iotg.so.1.1.0 | grep -i open62541-iotg-custom)
fi

# Link the libraries path to .so
if [[ "$1" == "--overwrite" ]]; then
    ln -sf libbpf.so.0.2.0 $libbpf_so_path
    ln -sf libopen62541-iotg.so.1.1.0 $open62541_so_path
fi

# Echo libraries path to user
echo -e "\n============================================="
echo -e "Installed Libraries Information:"
echo -e "============================================="
echo -e "===== libbpf ====="
echo -e "Library Version:"
echo -e "libbpf.so.0.2.0"
echo -e "\nLibrary Install Folder:"
echo -e "$libbpf_path"
echo -e "\n===== open62541 ====="
echo -e "Library Version:"
echo -e "libopen62541.so.1.1.0"
echo -e "\nLibrary Install Folder:"
echo -e "$open62541_path"

echo -e "\nSetup Successful."
echo -e "Please Re-login or source /etc/environment for libraries to link up"

# Solution for git clone fail
# 1. Please configure the proxy according to your proxy setting before git clone
# 2. Please configure the system date up-to-date before git clone
# 3. Please reboot your system before git clone

exit 0
