{
  "opcua_server": {
    "publisher_interface": "_PREPROCESS_STR_2nd_interface",
    "subscriber_interface": "_PREPROCESS_STR_interface",
    "use_xdp": true,
    "use_xdp_zc": true,
    "use_xdp_skb": false,
    "packet_count": 1000000,
    "cycle_time_ns": 400000,
    "polling_duration_ns": 0,
    "publishers": {
      "pub1": {
        "url": "opc.eth://aa-11-aa-11-aa-11:3.1",
        "pub_id": 2235,
        "dataset_writer_id": 62541,
        "writer_group_id": 101,
        "early_offset_ns": 50000,
        "publish_offset_ns": 250000,
        "socket_prio": 1,
        "two_way_data": true,
        "cpu_affinity": 2,
        "xdp_queue": 1
      }
    },
    "subscribers": {
      "sub1": {
        "url": "opc.eth://22-bb-22-bb-22-bb:3.1",
        "sub_id": 22,
        "subscribed_pub_id": 2234,
        "subscribed_dataset_writer_id": 62541,
        "subscribed_writer_group_id": 101,
        "offset_ns": 5000,
        "subscriber_output_file": "afxdp-rxtstamps.txt",
        "two_way_data": false,
        "cpu_affinity": 3,
        "xdp_queue": 1
      }
    }
  },
}
