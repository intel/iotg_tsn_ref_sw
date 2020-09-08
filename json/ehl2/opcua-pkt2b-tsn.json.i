{
  "custom_sync_b": {
    "interface": "_PREPROCESS_STR_interface.vlan",
    "interface2": "_PREPROCESS_STR_2nd_interface.vlan",
    "ignore_existing": true,
    "socket_prio": 1
  },
  "tc_group": [
    {
      "interface": "_PREPROCESS_STR_interface",
      "mqprio": {
        "handle": 100,
        "num_tc": 7,
        "queues": "1@0 1@1 1@2 1@3 1@4 1@5 1@6",
        "mapping": {
          "default": 0,
          "p1": 1,
          "p2": 2,
          "p3": 3,
          "p4": 4,
          "p5": 5,
          "p6": 6
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
          "vlan_priority": 6,
          "rx_hw_q": 2
        }
      ]
    },
    {
      "interface": "_PREPROCESS_STR_2nd_interface",
      "taprio": {
        "handle": 100,
        "num_tc": 7,
        "queues": "1@0 1@1 1@2 1@3 1@4 1@5 1@6",
        "time_elapsed": 3,
        "mapping": {
          "default": 0,
          "p1": 1,
          "p2": 2,
          "p3": 3,
          "p4": 4,
          "p5": 5,
          "p6": 6
        },
        "schedule": [
          {
            "gate_mask": "43",
            "duration": 1000000
          },
          {
            "gate_mask": "42",
            "duration": 1000000
          }
        ],
        "offload": false
      },
      "etf": [
        {
          "delta": 400000,
          "queue": 6,
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
        },
        {
          "vlan_priority": 6,
          "rx_hw_q": 2
        }
      ]
    }
  ]
}
