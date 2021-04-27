{
  "opcua_server": {
    "publisher_interface": "_PREPROCESS_STR_interface",
    "subscriber_interface": "_PREPROCESS_STR_interface",
    "use_xdp": false,
    "packet_count": 1000000,
    "cycle_time_ns": 1000000,
    "polling_duration_ns": 0,
    "publishers": {
      "pub1": {
        "url": "opc.eth://22-bb-22-bb-22-bb:3.6",
        "pub_id": 2234,
        "dataset_writer_id": 62541,
        "writer_group_id": 101,
        "early_offset_ns": 800000,
        "publish_offset_ns": 100,
        "publish_delay_sec": 3,
        "socket_prio": 6,
        "two_way_data": false,
        "cpu_affinity": 2,
        "xdp_queue": -1
      }
    },
    "subscribers": {}
  },
}
