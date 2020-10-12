import argparse
import os.path
import re
import sys
import time
import os
import json
import math

import helpers as h

# Dry run or not
# Make this a global so we don't have to pass it everywhere
# IS_DRY = False

def get_show_qdisc_cmd(interface):
    return 'tc qdisc show dev {}'.format(interface)

def delete_qdiscs(iface):
    #print('Deleting existing qdiscs')
    cmd = 'tc qdisc del dev {} root'.format(iface)
    proc = h.sh_run(cmd)
    h.sh_run('sleep 5')

def init_ingress(iface, show_qdisc_cmd):
    clear_rx_cmd = "tc qdisc del dev {} parent ffff:".format(iface)
    add_ingress_cmd = "tc qdisc add dev {} ingress".format(iface)

    output = h.sh_run(clear_rx_cmd)
    output = h.sh_run(add_ingress_cmd)

def set_vlanrx(iface, config, show_qdisc_cmd):

    vlan_priority = str(config['vlan_priority'])
    rx_hw_q = str(config['rx_hw_q'])

    set_vlanrx_cmd = "tc filter add dev {} ".format(iface)
    set_vlanrx_cmd += "parent ffff: protocol 802.1Q flower vlan_prio {} ".format(vlan_priority)
    set_vlanrx_cmd += "hw_tc {} ".format(rx_hw_q)

    output = h.sh_run(set_vlanrx_cmd)


def set_taprio(iface, maps, config, basetime, clkid):
    schedules = ""
    for each in config["schedule"]:
        schedules += "sched-entry S {} {} ".format(each['gate_mask'], str(each['duration']))

    num_tc = 0
    queues = 0
    handle = 0
    for each in config.keys():
        if each == "num_tc":
            num_tc = config["num_tc"]
        if each == "queues":
            queues = config["queues"]
        if each == "handle":
            handle = config["handle"]

    if not num_tc:
        #print( "num_tc not defined" )
        exit(1)
    if not queues:
        #print(  "queues not defined" )
        exit(1)
    if not handle:
        #print ( "handle not defined" )
        exit(1)

    # Final taprio command to run
    set_taprio_cmd = "tc -d qdisc replace dev {} ".format(iface)
    set_taprio_cmd += "parent root handle {} ".format(handle)
    set_taprio_cmd += "taprio num_tc {} map {} ".format(num_tc, maps)
    set_taprio_cmd += "queues {} base-time {} ".format(queues, basetime)
    set_taprio_cmd += "{} flags 0x2".format(schedules)

    # Parse hardware offload option
    # It's specified in json as "offload": true|false
    if 'offload' in config and config.get('offload'):
        set_taprio_cmd += ' offload 1'

    output = h.sh_run(set_taprio_cmd)

def set_mqprio(iface, maps, config):
    num_tc = 0
    queues = 0
    handle = 0
    for each in config.keys():
        if each == "num_tc":
            num_tc = config["num_tc"]
        if each == "queues":
            queues = config["queues"]
        if each == "handle":
            handle = config["handle"]

    if not num_tc:
        #print("num_tc not defined")
        exit(1)
    if not queues:
        #print("queues not defined")
        exit(1)
    if not handle:
        #print("handle not defined")
        exit(1)

    # Final mqprio command to run
    set_mqprio_cmd = "tc qdisc add dev {} ".format(iface)
    set_mqprio_cmd += "parent root handle {} ".format(handle)
    set_mqprio_cmd +="mqprio num_tc {} ".format(num_tc)
    set_mqprio_cmd +="map {} queues {} hw 0".format(maps,queues)
    output = h.sh_run(set_mqprio_cmd)

def set_etf(iface, clkid, config, show_cmd, use_taprio):
    # unpack config variable
    delta = str(config['delta'])

    # naming convention for queue is <parent>:<queue>
    # since <queue> naming starts from 1, we add 1 to queue.
    queue = str(config['queue'] + 1)

    # deadline_mode and offload is turn off by default
    deadline_mode = False
    offload = False

    # find the parent for specific qdisc
    cmd = 'HANDLE_ID="$(tc qdisc show dev {} | tr -d \':\' | awk \'NR==1{{print $3}}\')"'.format(iface)
    h.sh_run(cmd)

    # check if config specify deadline_mode or offload
    for each in config.keys():
        if each == 'deadline_mode':
            deadline_mode = config['deadline_mode']
        if each == 'offload':
            offload = config['offload']

    # generate etf command
    etf_default_cmd = 'tc qdisc replace dev {} parent $HANDLE_ID:{} etf '.format(iface, queue)
    etf_default_cmd += ' clockid {}'.format(clkid)
    etf_default_cmd += ' delta {}'.format(delta)
    if offload == True:
        etf_default_cmd += ' offload'
    if deadline_mode == True:
        etf_default_cmd += ' deadline_mode'

    # #print('Adding etf qdisc on queue {}...'.format(queue))
    output = h.sh_run(etf_default_cmd)
    # h.sh_run('sleep 2')

def set_mapping(scenario_config, use_mapping):
    maps = []
    maps.append(str(scenario_config[use_mapping]["mapping"]["default"]))
    maps = maps * 16
    # apply mapping for specific queue
    for each in scenario_config[use_mapping]["mapping"]:
        if each == 'default':
            continue
        priority = int(each[1:])
        queue = str(scenario_config[use_mapping]["mapping"][each])
        maps[priority] = queue
    maps = ' '.join(maps)
    return maps

def set_cbs(iface, scenario_config):
    handle = scenario_config["handle"]
    parent = scenario_config["parent"]
    queue = scenario_config["queue"] + 1
    sendslope = scenario_config["sendslope"]
    idleslope = scenario_config["idleslope"]
    hicredit = scenario_config["locredit"]
    locredit = scenario_config["locredit"]
    offload= scenario_config["offload"]

    # generate cbs command
    etf_default_cmd = 'tc qdisc replace dev {} '.format(iface)
    etf_default_cmd += 'handle {} parent {}:{} '.format(handle,parent,queue)
    etf_default_cmd += 'cbs idleslope {} sendslope {} '.format(idleslope,sendslope)
    etf_default_cmd += 'hicredit {} locredit {} '.format(hicredit,locredit)
    etf_default_cmd += 'offload {} '.format(offload)

    #print('Adding cbs qdisc on queue {}...'.format(queue))
    output = h.sh_run(etf_default_cmd)
    h.sh_run('sleep 5')

def process_tc_data(data):
    # If file is empty then we do nothing
    if len(data) is 0:
        return

    if not 'interface' in data:
        h.err_exit('Interface not found in tc config')

    interface = data.get('interface')

    delete_qdiscs(interface)

    # Moved from existing script
    use_scheduler = ''
    is_using_taprio = False
    if 'mqprio' in data: use_scheduler = 'mqprio'
    if 'taprio' in data:
        is_using_taprio = True
        use_scheduler = 'taprio'

    clock_id = 'CLOCK_TAI'
    if use_scheduler:
        #print("Setting up {} qdisc".format(use_scheduler))
        maps = set_mapping(data, use_scheduler)

        if use_scheduler == 'taprio':
            time_elapsed = data['taprio']['time_elapsed']
            base_time = '$(expr $(date +%s) + 5)000000000'
            #print('Base time set to {}s from now'.format(time_elapsed))
            set_taprio(interface, maps, data["taprio"], base_time, clock_id)

        elif use_scheduler == 'mqprio':
            set_mqprio(interface, maps, data["mqprio"])

    show_qdisc_cmd = get_show_qdisc_cmd(interface)

    if 'cbs' in data:
        #print('Setup CBS')
        set_cbs(interface, data["cbs"])

    if 'etf' in data:
        #print('Setup ETF')
        for each_config in data["etf"]:
            set_etf(interface, clock_id, each_config, show_qdisc_cmd,
                    is_using_taprio)

    if 'vlanrx' in data:
        #print('Setup VLAN RX steering')
        init_ingress(interface, show_qdisc_cmd)
        for each_config in data["vlanrx"]:
            set_vlanrx(interface, each_config, show_qdisc_cmd)

    if 'run_sh' in data:
        #print('Running raw shell commands, this shouldn\'t happen in prod')
        for cmd in data.get('run_sh'):
            h.sh_run(cmd)

    # #print(h.sh_run(show_qdisc_cmd).decode('utf8'))

def process_iperf3(obj):
    # kill all currently running iperf3
    h.sh_run('pkill iperf3')
    run_server = obj.get('run_server', False)
    client_target_add = str(obj.get('client_target_address'))
    cpu_aff = str(obj.get('cpu_affinity', 3))
    client_runtime = str(obj.get('client_runtime_in_sec', 5000))
    iperf3_log = "/var/log/iperf3_client.log"
    client_bw = str(obj.get('client_bandwidth_in_mbps', 1))

    # run the iperf3 server if server is enabled
    if run_server:
        iperf3_com = 'iperf3 -s -D --affinity {} &'.format(cpu_aff)
        h.sh_run(iperf3_com)

    # create the iperf3 client(udp) bash script if target server IP is given
    if client_target_add:
        client_iperf3_com = 'iperf3 -c {} --affinity {} -u -t {} --logfile {} -b {}M '.format(client_target_add, cpu_aff, client_runtime, iperf3_log, client_bw)
        f = open('iperf3-gen-cmd.sh','w')
        f.write(str(client_iperf3_com))
        f.close()

def process_ptp(obj):
    h.ensure_keys_exist('ptp', obj, 'interface')
    iface = obj.get('interface')
    ignore_existing = obj.get('ignore_existing', False)
    socket_prio = str(obj.get('socket_prio', 2))

    ## TODO sock prio or net prio
    if not ignore_existing: return
    h.sh_run('pkill ptp4l')

    # ptp4l -mP2Hi eth0 -f scripts/gPTP.cfg --step_threshold=2 --socket_priority 1
    arglist = ['taskset', '-c', '1', 'ptp4l', '-mP2Hi', iface, '-f',  'scripts/gPTP.cfg', '--step_threshold=2']
    arglist += ['--socket_priority', socket_prio]
    h.run_with_out(arglist, '/var/log/ptp4l.log')
    h.sh_run('sleep 30')

def process_phc2sys(obj):
    h.ensure_keys_exist('phc2sys', obj, 'clock', 'interface')
    clock = obj.get('clock')
    iface = obj.get('interface')
    ignore_existing = obj.get('ignore_existing', False)

    if not ignore_existing: return
    h.sh_run('pkill phc2sys')
    h.sh_run('sleep 2')

    # pmc -ub 0 -t 1 "SET GRANDMASTER_SETTINGS_NP clockClass 248 \
    #       clockAccuracy 0xfe offsetScaledLogVariance 0xffff    \
    #       currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 1 \
    #       ptpTimescale 1 timeTraceable 1 frequencyTraceable 0 timeSource 0xa0
    arglist = ['pmc', '-u', '-b', '0', '-t', '1', '"',
                'SET GRANDMASTER_SETTINGS_NP clockClass 248'
                ' clockAccuracy 0xfe offsetScaledLogVariance 0xffff currentUtcOffset 37'
                ' leap61 0 leap59 0 currentUtcOffsetValid 1 ptpTimescale 1 timeTraceable'
                ' 1 frequencyTraceable 0 timeSource 0xa0', '"']
    h.run_with_out(arglist, '/var/log/pmc.log')

    h.sh_run('sleep 2')

    # phc2sys -c CLOCK_REALTIME --step_threshold=1 -s eth0 \
    #       --transportSpecific=1 -O 0 -w -ml 7
    arglist = ['taskset', '-c', '1', 'phc2sys', '-c', 'CLOCK_REALTIME', '--step_threshold=1', '-s',
                iface, '--transportSpecific=1', '-O', '0', '-w', '-ml', '7']
    h.run_with_out(arglist, '/var/log/phc2sys.log')

def process_custom_a(obj):
    h.ensure_keys_exist(obj, 'interface2', 'interface')
    iface = obj.get('interface')
    iface2 = obj.get('interface2')
    ignore_existing = obj.get('ignore_existing', False)
    socket_prio = str(obj.get('socket_prio', 1))

    if not ignore_existing: return
    h.sh_run('pkill phc2sys')
    h.sh_run('pkill ptp4l')

    # ptp4l -mP2Hi eth0 -f scripts/gPTP.cfg --step_threshold=2 --socket_priority 1
    arglist = ['taskset', '-c', '1', 'ptp4l', '-mP2Hi', iface, '-f',  'scripts/gPTP.cfg', '--step_threshold=2']
    arglist += ['--socket_priority', '1']
    h.run_with_out(arglist, '/var/log/ptp4l.log')
    #print("Give 30 secs for ptp to sync")
    if not h.IS_DRY:
        h.sh_run('sleep 30')

    # pmc -ub 0 -t 1 "SET GRANDMASTER_SETTINGS_NP clockClass 248 \
    #       clockAccuracy 0xfe offsetScaledLogVariance 0xffff    \
    #       currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 1 \
    #       ptpTimescale 1 timeTraceable 1 frequencyTraceable 0 timeSource 0xa0
    arglist = ['pmc', '-u', '-b', '0', '-t', '1', '"',
                'SET GRANDMASTER_SETTINGS_NP clockClass 248'
                ' clockAccuracy 0xfe offsetScaledLogVariance 0xffff currentUtcOffset 37'
                ' leap61 0 leap59 0 currentUtcOffsetValid 1 ptpTimescale 1 timeTraceable'
                ' 1 frequencyTraceable 0 timeSource 0xa0', '"']
    h.run_with_out(arglist, '/var/log/pmc.log')

    # phc2sys -c CLOCK_REALTIME --step_threshold=1 -s eth0 \
    #       --transportSpecific=1 -O 0 -w -ml 7
    arglist = ['taskset', '-c', '1', 'phc2sys', '-c', 'CLOCK_REALTIME', '--step_threshold=1', '-s',
                iface, '--transportSpecific=1', '-O', '0', '-w', '-ml', '7']
    h.run_with_out(arglist, '/var/log/phc2sys.log')

def process_custom_b(obj):
    h.ensure_keys_exist(obj, 'interface2', 'interface')
    iface = obj.get('interface')
    iface2 = obj.get('interface2')
    ignore_existing = obj.get('ignore_existing', False)
    socket_prio = str(obj.get('socket_prio', 1))

    if not ignore_existing: return
    h.sh_run('pkill phc2sys')
    h.sh_run('pkill ptp4l')

    # ptp4l -mP2Hi eth0 -i eth2 -f scripts/gPTP.cfg --step_threshold=2 \
    #   --socket_priority 1 --boundary_clock_jbod=1
    arglist = ['taskset', '-c', '1', 'ptp4l', '-mP2Hi', iface, '-i', iface2 , '-f',  'scripts/gPTP.cfg', '--step_threshold=2']
    arglist += ['--socket_priority', '1']
    arglist += ['--boundary_clock_jbod=1']
    h.run_with_out(arglist, '/var/log/ptp4l.log')
    #print("Give 30 secs for ptp to sync")
    if not h.IS_DRY:
        h.sh_run('sleep 30')

    # pmc -ub 0 -t 1 "SET GRANDMASTER_SETTINGS_NP clockClass 248 \
    #       clockAccuracy 0xfe offsetScaledLogVariance 0xffff    \
    #       currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 1 \
    #       ptpTimescale 1 timeTraceable 1 frequencyTraceable 0 timeSource 0xa0
    arglist = ['pmc', '-u', '-b', '0', '-t', '1', '"',
                'SET GRANDMASTER_SETTINGS_NP clockClass 248'
                ' clockAccuracy 0xfe offsetScaledLogVariance 0xffff currentUtcOffset 0'
                ' leap61 0 leap59 0 currentUtcOffsetValid 1 ptpTimescale 1 timeTraceable'
                ' 1 frequencyTraceable 0 timeSource 0xa0', '"']
    h.run_with_out(arglist, '/var/log/pmc.log')

    # phc2sys -arrml 7 -f scripts/gPTP.cfg
    arglist = ['taskset', '-c', '1', 'phc2sys', '-arrml', '7', '-f', 'scripts/gPTP.cfg']
    h.run_with_out(arglist, '/var/log/phc2sys.log')

def main():
    parser = argparse.ArgumentParser(description='Configures the interface')
    parser.add_argument('config_file', help='Path to config file')
    parser.add_argument('-d', '--dry-run', dest='dry', default=False,
            action='store_true', help='Display commands without running them')
    args = parser.parse_args()
    cfg_path = args.config_file

    h.initialize()
    h.IS_DRY = args.dry

    if not os.path.isfile(cfg_path):
        #print('File {} not found'.format(cfg_path))
        exit(1)

    with open(cfg_path, 'r') as f:
        data = json.loads(f.read())

    if 'ptp' in data:
        process_ptp(data['ptp'])

    if 'phc2sys' in data:  process_phc2sys(data['phc2sys'])

    if 'custom_sync_a' in data:  process_custom_a(data['custom_sync_a'])
    if 'custom_sync_b' in data:  process_custom_b(data['custom_sync_b'])

    for each_tc in data["tc_group"]:
        process_tc_data(each_tc)

    if 'iperf3' in data:
        process_iperf3(data['iperf3'])

    h.teardown()

if __name__ == '__main__':
    main()
