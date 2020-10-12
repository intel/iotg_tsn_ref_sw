{
  "ptp": {
    "interface": "_PREPROCESS_STR_interface.vlan",
    "ignore_existing": true,
    "socket_prio": 1
  },
  "phc2sys": {
    "interface": "_PREPROCESS_STR_interface",
    "clock": "CLOCK_REALTIME",
    "ignore_existing": true
  },
  "tc_group": [
    {
      "interface": "_PREPROCESS_STR_interface",
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
          "vlan_priority": 6,
          "rx_hw_q": 2
        }
      ]
    }
  ]
}
