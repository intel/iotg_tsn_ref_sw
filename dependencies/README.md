# IOTG TSN Reference Software Dependencies Installer for Linux and Ubuntu

## Overview

The dependencies installers are to install dependencies for libbpf and open62541.

There are **three installer**, each with their own set of _examples_:

* **install_dependencies.sh** - Install dependencies for libbpf and libopen62541
  * ./install_dependencies.sh - Install dependencies in custom directory.
  * ./install_dependencies.sh --overwrite - Overwrite the original dependencies.

* **install_libbpf_overwrite.sh** - Only install dependency for libbpf
  * ./install_libbpf_overwrite.sh - Overwrite the original libbpf/

* **install_open62541_overwrite.sh** - Only install dependency for open62541
  * ./install_open62541_overwrite.sh - Overwrite the original open62541

## Compatibility

Currently supported systems are:

* Linux
* Ubuntu

### Disclaimer

* The dependencies installers only serves to install dependencies for libbpf and open62541

* This project is not for intended for production use.

* This project is intended to be used with specific platforms and bsp, other
  hardware/software combinations YMMV

* Users are responsible for their own products' functionality and performance.

## FAQ

If git clone fail, try the solution below:

* Please configure the proxy according to your proxy setting before git clone

* Please configure the system date up-to-date before git clone

* Please reboot your system before git clone

