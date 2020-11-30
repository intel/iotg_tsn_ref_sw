{
 "opcua_server": {
    "publisher_interface": "_PREPROCESS_STR_interface",
    "subscriber_interface": "_PREPROCESS_STR_interface",
    "use_xdp": false,
    "packet_count": 1000000,
    "cycle_time_ns": 1000000,
    "polling_duration_ns": 0,
    "publishers": {},
    "subscribers": {
      "sub1": {
        "url": "opc.eth://22-bb-22-bb-22-bb",
        "sub_id": 0,
        "subscribed_pub_id": 2234,
        "subscribed_dataset_writer_id": 62541,
        "subscribed_writer_group_id": 101,
        "offset_ns": 50000,
        "subscriber_output_file": "afpkt-rxtstamps.txt",
        "two_way_data": false,
        "cpu_affinity": 3,
        "xdp_queue": -1
      }
    }
  },
}
