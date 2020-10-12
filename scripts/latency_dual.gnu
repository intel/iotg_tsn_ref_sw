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

# Input files should be single-column integers with no headers present

# Example cmdline:
#	gnuplot -e "FILENAME='input.txt';FILENAME2='input2.txt'" latency_dual.gnu -p

reset

	set terminal pngcairo size 1920,1080
	set output 'plot_pic.png'

	set size 1,1
	set datafile missing'?'	# Skip invalid/missing data
	set locale "en_US.utf8"	# Set to en_US locale for thousands seperator

	set tmargin 5
	set bmargin 5
	set lmargin 15
	set rmargin 5

	set yrange [0:10000000]	#Legacy socket plot YMAX

	set grid
	set key center tmargin
	set border

	set style fill solid 0.25
	set boxwidth 0.5
	# set logscale y 10

	set title "Transmission latency from TX User-space to RX User-space"
	set xlabel "Packet count"
	set ylabel "Latency in nanoseconds"

	set multiplot layout 1,2

	stats FILENAME u 1 name "a" nooutput
	stats FILENAME2 u 1 name "b" nooutput

	min_us = a_min/1000
	max_us = a_max/1000
	avg_us = a_mean/1000

	set label front sprintf("Minimum: %d us", min_us) at graph 0.01, graph 0.95
	set label front sprintf("Maximum: %d us", max_us) at graph 0.01, graph 0.90
	set label front sprintf("Average: %d us", avg_us) at graph 0.01, graph 0.85

	plot FILENAME \
		using ($1) title "Legacy socket"  lc rgb "red" w points

	unset label

	set yrange [0:2000000]	#XDP socket plot YMAX

	min_us = b_min/1000
	max_us = b_max/1000
	avg_us = b_mean/1000

	set label front sprintf("Minimum: %d us", min_us) at graph 0.01, graph 0.95
	set label front sprintf("Maximum: %d us", max_us) at graph 0.01, graph 0.90
	set label front sprintf("Average: %d us", avg_us) at graph 0.01, graph 0.85

	plot FILENAME2 \
		using ($1) title "XDP socket"  lc rgb "red" w points

	unset multiplot
	unset output

	#Replot to GUI
	set terminal x11 size 1720,980
	set output
	set multiplot layout 1,2
	plot FILENAME \
		using ($1) title "Legacy socket"  lc rgb "red" w points

	plot FILENAME2 \
		using ($1) title "XDP socket"  lc rgb "red" w points

pause 10 #in case some one forgets to add -p
