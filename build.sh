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

showTsnRefHelp() {
cat << EOF
Usage: ./build.sh [-hxtVv]

-h,     -help,             --help             Display help

-t,     -disablexdptbs,    --disablexdptbs    Disable Intel specific XDP+TBS support. (By default XDP+TBS support is enabled)

-V,     -verbose,   --verbose   Run script in verbose mode.

-v,     -version,   --version   Show version.

EOF
}

showTsnRefVersion() {
    sed -n '/^TSNREFSW_PACKAGE_VERSION/p' ./run.sh
}

# Check OS distro
echo -e "\nBUILD.SH: Checking OS Distro"
os_distro=$(cat /etc/os-release | grep -w NAME | cut -c 6-)
echo "OS Distro: $os_distro"

# Check for default shell
echo -e "\nBUILD.SH: Checking for Bash shell"
if [ "$os_distro" == "\"Ubuntu"\" ]; then
	bash_shell=$(ls -la /usr/bin | grep -i 'bash' > /dev/null && echo 0 || echo 1)
else
	bash_shell=$(ls -la /bin/sh | grep -i 'bash' > /dev/null && echo 0 || echo 1)
fi

if [ $bash_shell = 0 ]; then
    echo "Bash shell: yes"
else
    echo "Bash shell: no"
    echo "Please install Bash shell (sudo apt-get install bash) in your system to proceed"
    exit 1
fi

# By default we enable xdp and xdp+tsb(when supported) intel specific implementation.
export version=0
export verbose=0
export ENABLE_XDP="--enable-xdp"
export ENABLE_XDPTBS="--enable-xdptbs"

options=$(getopt -l "help,disablexdptbs,verbose,version" -o "htVv" -a -- "$@")
eval set -- "$options"

while true
do
case $1 in
-h|--help)
    showTsnRefHelp
    exit 0
    ;;
-v|--version)
    showTsnRefVersion
    exit 0
    ;;
-t|--disablexdptbs)
    export ENABLE_XDPTBS=""
    ;;
-V|--verbose)
    export verbose=1
    set -xv # Set xtrace and verbose mode
    ;;
--)
    shift
    break;;
esac
shift
done

echo "./build.sh settings:"
echo "1. verbose = $verbose"
echo "2. enablexdp = $ENABLE_XDP"
echo "3. enablexdptbs = $ENABLE_XDPTBS"

touch Makefile.am configure.ac

# Ideally, just use git reset HEAD --hard
rm -rf  Makefile                \
        Makefile.in             \
        aclocal.m4              \
        autom4te.cache/         \
        compile                 \
        config.guess            \
        config.h.in             \
        config.log              \
        config.status           \
        config.sub              \
        configure               \
        depcomp                 \
        install-sh              \
        missing                 \
        src/.deps/              \
        src/opcua-tsn/.deps/    \
        tsq                     \
        txrx-tsn                \
        opcua-server            \
        *.png

echo -e "\nBUILD.SH: Configure"
autoreconf --install
./configure --prefix /usr $ENABLE_XDP $ENABLE_XDPTBS

if [ $? -ne 0 ]; then echo "Configure failed"; exit 1; fi

echo -e "\nBUILD.SH: Compile"
make -j 4

# Ideally, just use git reset HEAD --hard
echo -e "\nBUILD.SH: Clean up automake artifacts."
make distclean-compile  \
     distclean-generic  \
     distclean-hdr      \
     distclean-tags     \
     mostlyclean-am
