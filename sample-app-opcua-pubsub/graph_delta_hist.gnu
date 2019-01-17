###############################################################################
#
# Copyright (c) 2019, Intel Corporation
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

# Rx_stats is to call this file and pass 1 param: exp_ifdelay
# Example cmdline:
# 	gnuplot -e exp_ifdelay=250000 -p graph_delta_hist.gnu
reset
FILENAME="extracted_deltas.txt"

# Only plot if file is not empty
if (system("stat --print='%s' extracted_deltas.txt") > 600) {
	stats FILENAME skip 3 u 1 nooutput

	set terminal pngcairo size 1280,720
	set output 'plot_snapshot.png'

	set tmargin 5
	set bmargin 5
	set lmargin 15
	set rmargin 10
	set datafile missing'?'	# Skip invalid/missing data

	#Create function to calculate the height of each column/distribution
	n=1000		#how many samples/columns to have per plot
	min=exp_ifdelay-(exp_ifdelay*1)
	max=exp_ifdelay + exp_ifdelay
	width=(max-min)/n
	hist(x,width)= (x == NaN ? 1/0 : (width*floor(x/width)+width/2.0))

	if (STATS_min > 0) {
		set xrange [min:max]
	} else {
		set autoscale x
	}

	if (STATS_min == NaN && STATS_max == NaN) {
		quit
	}

	#set grid
	set key center tmargin
	set border

	set xtics min,(max-min)/10,max
	set logscale y 10
	set style histogram rows
	set style fill solid 0.25
	set boxwidth 0.5

	set xlabel "Inter-packet latency (ns)"
	set ylabel "Number of samples"
	set title "Inter-packet Latency Distribution"

	plot FILENAME \
		using (hist($1,width)) :(1) smooth frequency title "Traffic with LaunchTime" lc rgb "red" w impulse

	#Display the plot and pause in case someone forgets to specify -p
	set terminal x11
	set output
	replot
	pause 10
}

