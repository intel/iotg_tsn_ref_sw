{
  "opcua_server": {
    "publisher_interface": "_PREPROCESS_STR_interface",
    "subscriber_interface": "_PREPROCESS_STR_2nd_interface",
    "use_xdp": true,
    "use_xdp_zc": true,
    "use_xdp_skb": false,
    "packet_count": 1000000,
    "cycle_time_ns": 400000,
    "polling_duration_ns": 0,
    "publishers": {
      "pub1": {
        "url": "opc.eth://22-bb-22-bb-22-bb:3.2",
        "pub_id": 2234,
        "dataset_writer_id": 62541,
        "writer_group_id": 101,
        "early_offset_ns": 100000,
        "publish_offset_ns": 1000,
        "publish_delay_sec": 150,
        "socket_prio": 2,
        "two_way_data": false,
        "iperf_cpu_affinity": 2,
        "xdp_queue": 2
      }
    },
    "subscribers": {
      "sub1": {
        "url": "opc.eth://aa-11-aa-11-aa-11:3.2",
        "sub_id": 11,
        "subscribed_pub_id": 2235,
        "subscribed_dataset_writer_id": 62541,
        "subscribed_writer_group_id": 101,
        "offset_ns": 200000,
        "subscriber_output_file": "afxdp-rxtstamps.txt",
        "temp_file_dir": "/tmp",
        "two_way_data": true,
        "iperf_cpu_affinity": 3,
        "xdp_queue": 2
      }
    }
  }
}
