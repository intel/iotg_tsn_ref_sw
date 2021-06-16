{
  "ptp": {
    "interface": "_PREPROCESS_STR_interface.vlan",
    "gPTP_file": "gPTP_i225-1G.cfg",
    "ignore_existing": true
  },
  "phc2sys": {
    "interface": "_PREPROCESS_STR_interface",
    "clock": "CLOCK_REALTIME",
    "ignore_existing": true
  },
  "tc_group": [
    {
      "interface": "_PREPROCESS_STR_interface",
      "mqprio": {
        "handle": 100,
        "num_tc": 4,
        "queues": "1@0 1@1 1@2 1@3",
        "mapping": {
          "default": 0,
          "p1": 1,
          "p2": 2,
          "p3": 3
        }
      },
      "vlanrx": [
        {
          "vlan_priority": 1,
          "rx_hw_q": 1
        },
        {
          "vlan_priority": 2,
          "rx_hw_q": 2
        },
        {
          "vlan_priority": 3,
          "rx_hw_q": 3
        }
      ]
    }
  ]
}
