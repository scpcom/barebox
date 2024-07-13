#!/bin/sh
export CROSS_COMPILE=arm-linux-gnueabihf-
export ARCH=arm

mkdir -p BUILD_OUT

make distclean
make sequoia_defconfig
[ "X$CROSS_COMPILE" = "Xarm-linux-gnueabihf-" ] && sed -i s/'# CONFIG_AEABI is not set'/'CONFIG_AEABI=y'/g .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- MEM_SIZE=1G
cp barebox.bin BUILD_OUT/

make distclean
make sequoia-uloader_defconfig
[ "X$CROSS_COMPILE" = "Xarm-linux-gnueabihf-" ] && sed -i s/'# CONFIG_AEABI is not set'/'CONFIG_AEABI=y'/g .config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- MEM_SIZE=1G
cp uloader.bin BUILD_OUT/

cd BUILD_OUT
dd if=/dev/zero of=bareboxCombine.bin bs=65536 count=8
dd if=uloader.bin of=bareboxCombine.bin conv=notrunc
dd if=barebox.bin of=bareboxCombine.bin conv=notrunc bs=65536 seek=2
cd ..

echo OK
