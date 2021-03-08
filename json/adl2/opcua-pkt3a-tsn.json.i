{
  "custom_sync_a": {
    "interface": "_PREPROCESS_STR_interface.vlan",
    "interface2": "_PREPROCESS_STR_2nd_interface.vlan",
    "ignore_existing": true,
    "gPTP_file": "gPTP_SGMII-MV2110-1G.cfg",
    "socket_prio": 2
  },
  "iperf3": {
    "cpu_affinity": 0,
    "run_server": true,
    "client_target_address": "169.254.1.22",
    "client_runtime_in_sec": 5000,
    "client_bandwidth_in_mbps": 50
  },
  "tc_group": [
    {
      "interface": "_PREPROCESS_STR_interface",
      "taprio": {
        "handle": 100,
        "num_tc": 4,
        "queues": "1@0 1@1 1@2 1@3",
        "time_elapsed": 5,
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
      ],
      "etf": [
        {
          "delta": 400000,
          "queue": 3,
          "offload": true
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
        },
        {
          "vlan_priority": 3,
          "rx_hw_q": 3
        }
      ]
    }
  ]
}

