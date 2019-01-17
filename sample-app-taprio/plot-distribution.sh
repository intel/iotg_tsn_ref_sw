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

image=0
output_image='latency-distribution.png'
prio_arg=none
sourcefile='latencies.dat'
xmax=*
xmin=*

hi=1000000
lo=0

function usage {
	echo "Usage: ./plot-distribution.sh [-h] -p priority \
	      [-f source_file] [-g] \
	      [-m xmax_in_nanoseconds] [-n xmin_in_nanoseconds] \
	      [-o output_image_name]"
	echo "	-g	enable PNG image output"
	echo "	-h	show this message"
	exit -1
}

while getopts f:ghm:n:o:p: opt; do
	case ${opt} in
		f) sourcefile=${OPTARG} ;;
		g) image=1 ;;
		m) xmax=${OPTARG} ;;
		n) xmin=${OPTARG} ;;
		o) output_image=${OPTARG} ;;
		p) prio_arg=${OPTARG} ;;
		*) help=1 ;;
	esac
done

if [ -v help ]
then
	usage
fi

if [ ${prio_arg} == none ]
then
	echo "Must specify at least one priority to plot"
	usage
else
	# Parse all the priorities specified
	IFS=', ' read -r -a priorities <<< "$prio_arg"
fi

# Create plot command header
echo -n -e "\
	set title \"Inter-packet Latency Distribution\"\n\
	set xrange [${xmin}:${xmax}]\n\
	set xlabel \"Inter-packet latency (ns)\"\n\
	set logscale y
	set yrange [1:*]\n\
	set ylabel \"Number of latency samples\"\n\
	plot " >plotcmd

# Get latency data and calculate frequency for the first priority
if [ -v priorities[0] ]
then
	awk -v hi=$hi -v lo=$lo -v prio="${priorities[0]}" '{ if($1==prio && $2>lo && $2<hi) {print $2} } ' latencies.dat | sort | uniq -c | sort -nr >histogram
	echo -n "\"histogram$i\" using 2:1 w impulse lc rgb \"#154360\" title \"Priority ${priorities[0]}\"" >>plotcmd
fi

# Get latency data and calculate frequency for the second priority if specified
if [ -v priorities[1] ]
then
	awk -v hi=$hi -v lo=$lo -v prio="${priorities[1]}" '{ if($1==prio && $2>lo && $2<hi) {print $2} } ' latencies.dat | sort | uniq -c | sort -nr >histogram2
	echo -n ", \"histogram2$i\" using 2:1 w impulse lc rgb \"#ff5733\" title \"Priority ${priorities[1]}\"" >>plotcmd
fi

# Plot
gnuplot -persist "plotcmd$i"

# Plot to PNG
if [ ${image} == 1 ]
then
	echo -n -e "\n\
		set terminal pngcairo\n\
		set output \"${output_image}\"\n\
		replot" >>plotcmd
	gnuplot -persist "plotcmd$i"
fi

# Clean up
rm plotcmd
if [ -v priorities[0] ]
then
	rm histogram
fi
if [ -v priorities[1] ]
then
	rm histogram2
fi
