#!/bin/bash

###############################################################################
#
# Copyright (c) 2018, Intel Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
#  3. Neither the name of the copyright holder nor the names of its
#     contributors may be used to endorse or promote products derived from
#     this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
###############################################################################

set -e

def_num_samples=100
def_prio_args=none
def_refresh_time=1
def_source_file='data_graph.dat'
def_ymax=100000
def_ymin=0
def_VLAN_prio=none
def_VLAN_prio2=none

function usage {
	echo "Usage: ./plot.sh -p priority [-h]\
	      [-m ymax_in_nanoseconds] [-n ymin_in_nanoseconds]"
	echo "	-h	show this message"
	exit -1
}

while getopts m:n:p: opt; do
	case ${opt} in
		m)	def_ymax=${OPTARG} ;;
		n)	def_ymin=${OPTARG} ;;
		p)	def_prio_args=${OPTARG} ;;
		*)	help=1 ;;
	esac
done

if [ -v help ]
then
	usage
fi

if [ ${def_prio_args} == none ]
then
	echo "Must specify at least one priority to plot"
	usage
	exit -1
else
	IFS=', ' read -r -a priorities <<< "$def_prio_args"
fi

if [ -v priorities[0] ]
then
	def_VLAN_prio=priorities[0]
fi

if [ -v priorities[0] ]
then
	def_VLAN_prio2=priorities[1]
fi

if [ -z "$YMAX" ]
then
	export YMAX=$def_ymax
fi

if [ -z "$YMIN" ]
then
	export YMIN=$def_ymin
fi

if [ -z "$SAMPLES" ]
then
	export SAMPLES=$def_num_samples
fi

if [ -z "$REFRESH" ]
then
	export REFRESH=$def_refresh_time
fi

if [ -z "$FILE" ]
then
	export FILE=$def_source_file
fi

if [ -z "$VLANPRIO"]
then
	def_VLAN_offset=$(($def_VLAN_prio * 3))
	export VLANOFFSET=$(($def_VLAN_offset + 1))
	export VLANPRIO=$(($def_VLAN_prio))
fi

if [ -z "$VLANPRIO2"]
then
	def_VLAN_offset2=$(($def_VLAN_prio2 * 3))
	export VLANOFFSET2=$(($def_VLAN_offset2 + 1))
	export VLANPRIO2=$(($def_VLAN_prio2))
fi

echo Latency Y-axis max: $YMAX
echo Latency Y-axis min: $YMIN
echo Sample to plot: $SAMPLES
echo Source log file: $FILE
echo Refresh time: $REFRESH
echo TSN Priority: $VLANPRIO, $VLANPRIO2

gnuplot graph.gnu
