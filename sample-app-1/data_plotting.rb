#!/usr/bin/env ruby
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

#------------------------------------------------------------------------------
#
# Add all required libraries and Ruby files here
#
#------------------------------------------------------------------------------
require 'bigdecimal'
require 'bigdecimal/util'

#------------------------------------------------------------------------------
#
# This method will scan input file and extract raw timestamp of each device:
#
# 1. Search for the lines that begin with "server#" and push the subsequent
#    data to array @@raw_data.
# 2. Assign GrandMaster's first timestamp (decimal points omitted) to $master
#    and subtract all timestamps with $master. Results will be pushed to subx.
# 3. Round up subx up to one decimal point and push to x_temp. This will be the
#    data for x-axis of the graph. subx will be subtracted with @@x_data and
#    push to y_temp. This will be the data for the y-axis of the graph.
#
#------------------------------------------------------------------------------

def extract_data(server_num, source_file, max_samples)
	raw_data = Array.new(server_num){[]}
	$x = Array.new(server_num){[]}
	$y = Array.new(server_num){[]}
	x_temp = 0
	y_temp = 0

	#eg: server#0: 1464345445.500000030
	regex = Regexp.new(/server#(?<server>\d{1}): (?<data>\d(.+))/)

	#remove 'nil' created when initialize raw_data
	server_num.times {|i| raw_data[i - 1].drop(0)}

	#puts "Analyzing file #{source_file}..."
	File.foreach(source_file).with_index do |line|
		if read = line.match(regex)
			server, data = read.captures
			server = server.to_i
			if (server < server_num)
				data = data.chomp
				raw_data[server] << data
			else
				puts "Mismatch : MAX_SERVER & data collected"
				shut_down
			end
		end
	end

	# check if any of the server is not connected
	# and causes array to be empty
	for i in 0..(server_num - 1)
		if raw_data[i][0] == nil
			puts "Server#{i} not connected"
		end
	end

	# write Time error in file
	File.open("plot_time_error.dat", 'a+') { |f|

		if $last_plot_index == 0
			$last_plot_index = raw_data[0].length - 1
		end

		if (raw_data[0].length == raw_data[1].length)
			$cur = raw_data[0].length - 1
		elsif (raw_data[0].length > raw_data[1].length)
			$cur = raw_data[1].length - 1
		else
			$cur = raw_data[0].length - 1
		end

		for i in $last_plot_index..$cur

			diff = ((raw_data[1][i].to_d * 1000000000).to_i - (raw_data[0][i].to_d * 1000000000).to_i).to_i
			if diff < 300000
				if diff > -300000
					f.puts("#{i}\t#{diff}")
				end
			end
			$xmax = i
			$xmin = $xmax - 100
		end
		$last_plot_index = $cur
	}
end

def shut_down
	puts "Exiting..."
	sleep 1
	exit
end

max_servers = ARGV[0].to_i
source_file = ARGV[1]
max_samples = ARGV[2].to_i

# delete temporary file
File.delete("plot_time_error.dat") if File.exist?("plot_time_error.dat")

half_max_samples = max_samples / 2
pipe = IO.popen('gnuplot -p', 'r+')
pipe.puts "set xlabel 'Time (sec)'"
pipe.puts "set ylabel 'Time Error (nsec)'"
pipe.puts "set grid"
$xmin = 1
$xmax = 10
$last_plot_index = 0
$cur = 0
loop do
	extract_data(max_servers, source_file, max_samples)
	pipe.puts "set xrange[#{$xmin}:#{$xmax}]"
	pipe.puts "plot 'plot_time_error.dat' using 1:2 with linespoints linestyle 1 title ('Time Error')"
	sleep(1)
	Signal.trap("INT") {
		puts "INT"
		shut_down
	}
	Signal.trap("TERM") {
		puts "TERM"
		shut_down
	}
end
shut_down
