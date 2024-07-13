Modification of or tampering with the product, including but not limited to any Open Source Software, is solely at Your own risk. Western Digital is not responsible for any such modification or tampering. Western Digital will not support any product in which You have or have attempted to modify the software or hardware supplied by Western Digital.
----------------------------------------------------------


Barebox Build Instructions:

Prepare build environment
------------------------------------------------------
- untar provided toolchain to a directory (see ./toolchain/arm-cortex_a9-linux-gnueabi-4.7.2.60.tgz)
  If you wish to rebuild toolchain see instructions in ./toolchain/WD-README.txt
- export PATH=</path/to/toolchain>/bin:$PATH
- export CROSS_COMPILE=arm-cortex_a9-linux-gnueabi-
- export ARCH=arm
- mkdir BUILD_OUT

Build barebox
------------------------------------------------------
- make distclean
- make sequoia_defconfig
- make
- cp barebox.bin BUILD_OUT/

Build uloader
------------------------------------------------------
- make distclean
- make sequoia-uloader_defconfig
- make
- cp uloader.bin BUILD_OUT/

Build Barebox Full Image
------------------------------------------------------
- cd BUILD_OUT
- dd if=/dev/zero of=bareboxCombine.bin bs=65536 count=8
- dd if=uloader.bin of=bareboxCombine.bin conv=notrunc
- dd if=barebox.bin of=bareboxCombine.bin conv=notrunc bs=65536 seek=2

Install Barebox
------------------------------------------------------
The final image bareboxCombine.bin can only be written to the SPI Flash memory using an appropriate JTAG programmer. 
Modification of or tampering with the product, including but not limited to any Open Source Software, is solely at Your own risk. Western Digital is not responsible for any such modification or tampering. Western Digital will not support any product in which You have or have attempted to modify the software or hardware supplied by Western Digital.
