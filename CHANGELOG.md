# Changelog
All notable changes to this tsn ref sw  will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

NOTE: The ChangeLog is only included in the release starting v0.8.22.
For prior version, please refer to the tag commit message. Sorry guys.

## [0.8.22] - 2022-02-09
- Support for kernel 5.1x in the scripts
- Changes json config to support EHL 2-port opcua functionalities
  for kernel 5.10 and above.
- Configuration changes for vs1 for EHl kernel 5.10 and above

## [0.8.23] - 2022-02-18
- missing configuration for rpl opcua-pkt1 is added.
- vs1x rpl default configuration is changed.
- added check for gnuplot availability.
- check for log folder availability. Force creation if missing.

## [0.8.24] - 2022-03-08
- TBS calculation (avg and stddev)
- Additional column for U2U latency stddev
- GCL/taprio configuration printout
- timesync stat printout upon ptp4l/phc2sys logs availability
- XDP opcua queue change to q3 for adl2 and rpl2

## [0.8.25] - 2022-04-19
- Add README_faq.md (for tips on errors etc)
- Update all the other readme-s
- Add default shell checking, set up vlan interface, fix syntax error

## [0.8.26] - 2022-05-12
- ISDM: add CODEOWNERS and pull_request_template.md
- ADL-N enablement
- Additional column for U2U coefficient variance
- Additional column for TBS coefficient variance

## [0.8.27] - 2022-06-13
- ADL-X vs1 iperf speed change

## [0.8.28] - 2022-08-19
- RPL-P enablement
- Install scrub/filter to the raw data
- Improve logic for manage systemctl restart

## [0.8.29] - 2022-09-02
- Update TODO.md
- Code refactoring on napi deferral's part to increase performance
  (smaller latency) for each difference kernels

## [0.8.30] - 2022-09-28
- ADL-N support for SKU5 that only has 2 cores
- Change default gPTP settings for ADL-N to TI-PHY
- Change default gPTP settings for RPL-P to TI-PHY
- Change systemctl try-reload-or-restart to restart

## [0.9.0] - 2022-11-03
- Add support for no_xdp and no_xdp_tbs mode
- Publish data on af_packet when af_xdp is not available in vs1
- Decoupling XDP_TBS related configuration from OPC-UA server code
