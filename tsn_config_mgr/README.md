# TSN configuration manager

## Overview

This project is a tsn configuration manager. It is mainly written in python.

The tsn_cm daemon listens to configuration file (form of xml/yaml) drop in a
specified folder. Once dropped, tsn_cm will parse it and create an equivalent
tc command to be executed on the machine.

tsn_cm should be run in the end station that has a TSN-Hw card such as i226 card.

## Compatibility
```
Currently supported hardwares are:

	* i226

Currently supported systems are:

	* Ubuntu
```

## Important

## Dependencies

### General

#### *For system shell:*
```
# Bash are required as system shell in order to compile and run TSN Reference Software.
# Install bash using command below:
	sudo apt-get install bash
```

## Build and install

```
    cd <tsn_config_mgr_directory>
    python3 ./setup.py
```

## Documentation


## Disclaimer

* This project only serves to demonstrate TSN functionality and its
  usage on supported platforms and their environments.

* This project is not for intended for production use.

* This project is intended to be used with specific platforms and bsp, other HW/SW combinations YMMV

* Users are responsible for their own products' functionality and performance.

## License

Refer to [LICENSE](LICENSE)

## FAQ

