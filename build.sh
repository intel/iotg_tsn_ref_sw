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

# Check for default shell
echo -e "\nBUILD.SH: Checking Default Shell"
default_shell=$(echo $(realpath /usr/bin/sh) | cut -c 10-)
if [ $default_shell = "bash" -o $default_shell = "sh" ]; then
    echo "Default Shell: $default_shell (Valid)"
else
    echo "Default Shell: $default_shell (Invalid)"
    echo "Please change default shell to ' bash ' OR ' sh ' to proceed"
    exit 1
fi

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
./configure --prefix /usr --with-open62541-headers=/usr/include/open62541
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
