{
  "custom_sync_a": {
    "interface": "_PREPROCESS_STR_interface.vlan",
    "interface2": "_PREPROCESS_STR_2nd_interface.vlan",
    "ignore_existing": true,
    "gPTP_file": "gPTP_SGMII-MV2110-1G.cfg",
    "socket_prio": 2
  },
  "tc_group" : [
    {
      "interface": "_PREPROCESS_STR_interface",
      "taprio": {
        "handle": 100,
        "num_tc": 4,
        "queues": "1@0 1@1 1@2 1@3",
        "time_elapsed": 3,
        "mapping": {
          "default": 0,
          "p1": 1,
          "p2": 2,
          "p3": 3
        },
      "schedule": [
          {
            "gate_mask": "0e",
            "duration": 500000
          },
          {
            "gate_mask": "0f",
            "duration": 500000
          }
        ],
        "offload": false
      },
      "etf": [
        {
          "delta": 100000,
          "queue": 3,
          "offload": true
        }
      ],
      "vlanrx": [
        {
          "vlan_priority": 1,
          "rx_hw_q": 1
        },
        {
          "vlan_priority": 2,
          "rx_hw_q": 2
        }
      ]
    },
    {
      "interface": "_PREPROCESS_STR_2nd_interface",
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
        }
      ],
      "run_sh": [
        "ethtool -K _PREPROCESS_STR_interface rxvlan off",
        "ethtool -K _PREPROCESS_STR_2nd_interface rxvlan off"
      ]
    }
  ]
}
