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

# Plotting graph for qbv_demo
# - Use plot.sh to run this script. Configuration is done in the bash script
#
# Data input: <Time> <TSN seq> <TSN Latency> <TSN Packet loss>

done = 0
bind all 'c' 'done = 1'
while (!done) {
	refresh=system("echo $REFRESH")			# Store env variable into local variable
	ymax=system("echo $YMAX")			# Store env variable into local variable
	ymin=system("echo $YMIN")			# Store env variable into local variable
	tsn_data_offset=system("echo $VLANOFFSET")
	vprio=system("echo $VLANPRIO")

	tsn_data_offset_latency = tsn_data_offset + 2
	tsn_data_offset_lost= tsn_data_offset + 3
	tsn_latency=0
	tsn_lost=0

	tsn_data_offset2=system("echo $VLANOFFSET2")
	vprio2=system("echo $VLANPRIO2")
	tsn_data_offset_latency2 = tsn_data_offset2 + 2
	tsn_data_offset_lost2= tsn_data_offset2 + 3
	tsn_latency2=0
	tsn_lost2=0


	set multiplot layout 2,2
	set autoscale
	set grid
	unset log                              # remove any log-scaling
	unset label                            # remove any previous labels
	set key center tmargin

	# TSN Latency
	set border
	set xtics
	set ytics
	set xlabel 'Time (ms)' textcolor lt 3
	set ylabel 'Latency (ns)' textcolor lt 3
	stats  '< tail -n $SAMPLES $FILE|head -n $[SAMPLES-2]'  using 1:(tsn_latency=column(tsn_data_offset_latency)) nooutput
	plot [] [ymin:ymax] '< tail -n $SAMPLES $FILE|head -n $[SAMPLES-2]' using 1:(column(tsn_data_offset_latency)) with linespoints title sprintf('TSN VLAN Priority %s Latency', vprio)
	# TSN packet lost
	set ylabel 'Packet lost count' textcolor lt 3
	stats  '< tail -n $SAMPLES $FILE|head -n $[SAMPLES-2]'  using 1:(tsn_lost=column(tsn_data_offset_lost)) nooutput
	unset border
	unset xtics
	unset ytics
	unset xlabel
	unset ylabel
	plot [0:10] [100:200] '< echo 0 0' with linespoints lc rgb "white" notitle

	# TSN #2 Latency
	set border
	set xtics
	set ytics
	set xlabel 'Time (ms)' textcolor lt 3
	set ylabel 'Latency (ns)' textcolor lt 3
	stats  '< tail -n $SAMPLES $FILE|head -n $[SAMPLES-2]'  using 1:(tsn_latency2=column(tsn_data_offset_latency2)) nooutput
	plot [] [ymin:ymax] '< tail -n $SAMPLES $FILE|head -n $[SAMPLES-2]' using 1:(column(tsn_data_offset_latency2)) with linespoints title sprintf('TSN VLAN Priority %s Latency', vprio2)
	# TSN #2 packet lost
	set ylabel 'Packet lost count' textcolor lt 3
	stats  '< tail -n $SAMPLES $FILE|head -n $[SAMPLES-2]'  using 1:(tsn_lost2=column(tsn_data_offset_lost2)) nooutput
	unset border
	unset xtics
	unset ytics
	unset xlabel
	unset ylabel
	plot [0:10] [0:10] '< echo 0 0' with linespoints lc rgb "white" notitle

	pause refresh
	replot
	reread
}
unset multiplot

