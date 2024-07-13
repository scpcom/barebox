Modification of or tampering with the product, including but not limited to any Open Source Software, is solely at Your own risk. Western Digital is not responsible for any such modification or tampering. Western Digital will not support any product in which You have or have attempted to modify the software or hardware supplied by Western Digital.

arm-cortex_a9-linux-gnueabi toolchain Build Instructions:

Prepare build environment
------------------------------------------------------
- download and install following tools (apt-get install <package> || yum install <package>):
	* scons
	* autoconf
	* bison
	* flex
	* gperf
	* texinfo
	* patch
	* gawk
	* libtool >= 1.5.26...
	* libncurses5-dev
	* g++
- Download crosstool-ng to the build_tools/crosstool-ng directory.
  crosstool-ng download URL: http://crosstool-ng.org/download/crosstool-ng/crosstool-ng-1.16.0.tar.bz2

Build crosstool-ng
------------------------------------------------------
- cd build_tools/crosstool-ng
- scons
- mkdir /xtools
- cp -r crosstool-ng /xtools/

build toolchain
------------------------------------------------------
- cd build_tools/compilers
- modify SConstruct to build only arm-cortex_a9-linux-gnueabi and save.
- cd arm-cortex_a9-linux-gnueabi/
- tar -xvf arm-cortex_a9-linux-gnueabi.tgz
- cd ..
- scons
- built toolchain will be in build_tools/arm-cortex_a9-linux-gnueabi/Results/
