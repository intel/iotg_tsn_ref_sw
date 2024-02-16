{
  "ptp": {
    "interface": "_PREPROCESS_STR_interface.vlan",
    "gPTP_file": "gPTP_TI-1G.cfg",
    "ignore_existing": true,
    "ptp_cpu_affinity": 1
  },
  "phc2sys": {
    "interface": "_PREPROCESS_STR_interface",
    "clock": "CLOCK_REALTIME",
    "ignore_existing": true,
    "ptp_cpu_affinity": 1
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
      "etf": [
        {
          "delta": 400000,
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
        },
        {
          "vlan_priority": 3,
          "rx_hw_q": 3
        }
      ]
    }
  ]
}
