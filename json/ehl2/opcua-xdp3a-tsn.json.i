{
  "custom_sync_a": {
    "interface": "_PREPROCESS_STR_interface.vlan",
    "interface2": "_PREPROCESS_STR_2nd_interface.vlan",
    "ignore_existing": true,
    "gPTP_file": "gPTP_RGMII-MV1510-1G.cfg",
    "socket_prio": 1
  },
  "iperf3": {
    "cpu_affinity": 0,
    "run_server": true,
    "client_target_address": "169.254.1.22",
    "client_runtime_in_sec": 5000,
    "client_bandwidth_in_mbps": 200
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
            "gate_mask": "43",
            "duration": 200000
          },
          {
            "gate_mask": "42",
            "duration": 200000
          }
        ],
        "offload": false
      },
      "etf": [
        {
          "delta": 100000,
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
    },
    {
      "interface": "_PREPROCESS_STR_2nd_interface",
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
      ],
      "run_sh": [
        "ethtool -K _PREPROCESS_STR_interface rxvlan off",
        "ethtool -K _PREPROCESS_STR_2nd_interface rxvlan off"
      ]
    }
  ]
}
