#!/bin/sh
export CROSS_COMPILE=arm-linux-gnueabihf-
export ARCH=arm
export MEM_SIZE=1G

mkdir -p BUILD_OUT

make distclean
make comcerto-2k_evm_defconfig
[ "X$CROSS_COMPILE" = "Xarm-linux-gnueabihf-" ] && sed -i s/'^# CONFIG_AEABI is not set'/'CONFIG_AEABI=y'/g .config
make
cp barebox.bin BUILD_OUT/

make distclean
make comcerto-2k_evm_uloader_defconfig
[ "X$CROSS_COMPILE" = "Xarm-linux-gnueabihf-" ] && sed -i s/'^# CONFIG_AEABI is not set'/'CONFIG_AEABI=y'/g .config
make
cp uloader.bin BUILD_OUT/

scripts/bareboxenv -s -p 0x20000 arch/arm/boards/comcerto-evm/env/ env.bin
cp env.bin BUILD_OUT/

cd BUILD_OUT
dd if=/dev/zero of=bareboxCombine.bin bs=65536 count=8
dd if=uloader.bin of=bareboxCombine.bin conv=notrunc
dd if=barebox.bin of=bareboxCombine.bin conv=notrunc bs=65536 seek=2
cd ..

echo OK
