#!/usr/bin/env python2.7

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

import argparse
import os.path
import re
import subprocess
import sys
import time
import os

def run_cmd(cmd):
    """
    run_cmd - Run shell command from code
    @cmd: Command to run
    """
    cmd = cmd.replace('\n', '')
    with open(os.devnull, 'w') as silent:
        process = subprocess.Popen(cmd, stderr=silent, stdout=subprocess.PIPE, shell=True)
    output, error = process.communicate()
    return output

def generate_outfile(filename, data):
    """
    Generate output file that contains cycle base time
    @filename: Output file name to be generated
    @data: Base time to write
    """
    file = open(filename, 'w+')
    file.write(data)
    file.close()

def get_qdisc_parent(cmd, token):
    output = run_cmd(cmd)
    for item in output.split('\n'):
        if token in item:
            res = item.strip().split()
            return res[res.index(token) + 1]

def map_taprio(iface, maps, sched_file, bt, clkid):
    # Final taprio command to run

    # use sched_file content to generate sched-entry entries for TC.
    f = open(sched_file,"r")
    sched_list = f.readlines()
    f.close()
    schedules = ""
    for each in sched_list:
        schedules += " sched-entry {}".format(each.replace("\n"," "))

    map_taprio_cmd = 'tc -d qdisc replace dev ' + iface + ' parent root handle 100 taprio num_tc 4' + \
            ' map ' + maps + \
            ' queues 1@0 1@1 1@2 1@3' + \
            ' base-time ' + bt + \
            ' {} ' + \
            ' clockid ' + clkid
    output = run_cmd(map_taprio_cmd.format(schedules))

def map_mqprio(iface, maps):
    # Final mqprio command to run
    map_mqprio_cmd = 'tc qdisc add dev ' + iface + ' parent root mqprio num_tc 4' + \
            ' map ' + maps + \
            ' queues 1@0 1@1 1@2 1@3' + \
            ' hw 0'
    output = run_cmd(map_mqprio_cmd)

def set_etf(iface, clkid, delta, prio, show_cmd, use_taprio):
    if use_taprio is True:
        parent = get_qdisc_parent(show_cmd, 'taprio')
    else:
        parent = get_qdisc_parent(show_cmd, 'mqprio')

    if not parent:
        exit(1)

    # Etf command with deadline mode on. Will be overwritten if specified in map_file
    etf_deadline_cmd = ''
    # Default etf command. Hardware offload is turned on by default
    etf_default_cmd = 'tc qdisc replace dev ' + iface + ' parent ' + parent + '1 etf' + \
              ' clockid ' + clkid + \
              ' delta ' + delta + \
              ' offload'

    # Run etf on queue 0 if applicable
    if len(prio[0]) >= 3:
        print 'Adding etf qdisc on queue 0...'
        if len(prio[0]) == 4:
            tmp = etf_default_cmd.split(' ')
            tmp[11] = prio[0][3]
            etf_default_cmd = ' '.join(tmp)
        if prio[0][2] == 'etf':
            output = run_cmd(etf_default_cmd)
        elif prio[0][2] == 'etf_deadline':
            etf_deadline_cmd = etf_default_cmd + ' deadline_mode'
            output = run_cmd(etf_deadline_cmd)
        time.sleep(5)

    # Run etf on queue 1 if applicable
    if len(prio[1]) >= 3:
        print 'Adding etf qdisc on queue 1...'
        if len(prio[1]) == 4:
            tmp = etf_default_cmd.split(' ')
            tmp[11] = prio[1][3]
            etf_default_cmd = ' '.join(tmp)
        tmp = etf_default_cmd.split(' ')
        tmp[6] = parent + '2'
        etf_default_cmd = ' '.join(tmp)
        if prio[1][2] == 'etf':
            output = run_cmd(etf_default_cmd)
        elif prio[1][2] == 'etf_deadline':
            etf_deadline_cmd = etf_default_cmd + ' deadline_mode'
            output = run_cmd(etf_deadline_cmd)
        time.sleep(1)

def main():
    # Clock type defaulted to CLOCK_TAI
    clock_id = 'CLOCK_TAI'
    # ETF qdisc delta default value. Will be overwritten if specified in queue_map file
    delta = '200000'
    # Default hardware queue for all priorities are queue 3
    maps = ['3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3', '3']
    # TODO
    priorities = []
    use_taprio = False

    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--interface', help='interface name', required=True)
    parser.add_argument('-q', '--queue-map', help='priority-to-queue mapping file name', required=True)
    parser.add_argument('-e', '--time-elapsed', help='seconds elapsed in the future to start the schedule', type=int)
    parser.add_argument('-g', '--gate-schedule', help='gate schedule file name for TAPrio')
    args = parser.parse_args()

    # Interface name based on user input
    interface = args.interface
    # Priority to queue mapping config file based on user input
    queue_map_file = args.queue_map
    # Base time offset based on user input
    time_elapsed = args.time_elapsed
    # GCL file name based on user input
    schedule_file = args.gate_schedule

    # Check if priority-queue mapping file is valid
    if os.path.isfile(args.queue_map) == False:
        print args.queue_map + ' is not a valid file'
        exit(1)

    # Check if base time offset is less than 30s
    if args.time_elapsed < 3:
        print 'Choose time elapsed value value larger than 30s'
        exit(1)

    # Shell command to delete all existing qdisc configuration on the interface
    delete_qdisc_cmd = 'tc qdisc del dev ' + interface + ' root &>/dev/null'
    # Shell command to get the current time plus the base time offset
    get_basetime_cmd = 'i=$((`date +%s%N` + (' + str(time_elapsed) + ' * 1000000000))) ; ' + \
            'echo $(($i - ($i % 1000000000)))'
    # Shell command to get existing qdisc configuration on the interface
    show_qdisc_cmd = 'tc qdisc show dev ' + interface

    # Open priority-queue mapping config file and extract the priorities
    with open(queue_map_file) as file:
        for line in file:
            if line[0] == '#':
                continue
            priorities.append(line.split())

    # Arrange queue assignment to priority in tc qdisc command format
    for i in range(len(priorities)):
        index = int(priorities[i][0])
        maps[index] = priorities[i][1]
    maps = ' '.join(maps)

    # Delete any existing qdiscs
    print 'Deleting any existing qdisc...'
    run_cmd(delete_qdisc_cmd)
    time.sleep(5)

    # Generate base time output file
    base_time = run_cmd(get_basetime_cmd).replace('\n', '')
    generate_outfile('base_time', base_time)

    if schedule_file:
        use_taprio = True
        # Check if scheduler file is valid
        if os.path.isfile(schedule_file) == False:
            print schedule_file + ' is not a valid file'
            exit(1)
        map_taprio(interface, maps, schedule_file, base_time, clock_id)
    else:
        map_mqprio(interface, maps)

    set_etf(interface, clock_id, delta, priorities, show_qdisc_cmd, use_taprio)

    # Success message
    if time_elapsed:
        print 'Base time set to ' + str(time_elapsed) + 's from now (ns): ' + base_time
    print run_cmd(show_qdisc_cmd)

if __name__ == '__main__':
    main()
