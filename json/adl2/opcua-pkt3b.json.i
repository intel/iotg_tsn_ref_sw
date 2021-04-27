{
 "opcua_server": {
    "subscriber_interface": "_PREPROCESS_STR_interface",
    "publisher_interface": "_PREPROCESS_STR_2nd_interface",
    "use_xdp": false,
    "packet_count": 500000,
    "cycle_time_ns": 2000000,
    "polling_duration_ns": 0,
    "publishers": {
      "pub1": {
        "url": "opc.eth://aa-11-aa-11-aa-11:3.3",
        "pub_id": 2234,
        "dataset_writer_id": 62541,
        "writer_group_id": 101,
        "early_offset_ns": 750000,
        "publish_offset_ns": 1000100,
        "publish_delay_sec": 3,
        "socket_prio": 3,
        "two_way_data": true,
        "cpu_affinity": 2,
        "xdp_queue": -1
      }
    },
    "subscribers": {
      "sub1": {
        "url": "opc.eth://22-bb-22-bb-22-bb",
        "sub_id": 0,
        "subscribed_pub_id": 2234,
        "subscribed_dataset_writer_id": 62541,
        "subscribed_writer_group_id": 101,
        "offset_ns": 50000,
        "subscriber_output_file": "afpkt-rxtstamps.txt",
        "temp_file_dir": "/dev/shm",
        "two_way_data": false,
        "cpu_affinity": 3,
        "xdp_queue": -1
      }
    }
  },
}
