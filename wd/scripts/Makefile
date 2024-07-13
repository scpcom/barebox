#HOST_EXTRACFLAGS += -l$(srctree)/lib
#obj-y                           += ../../lib/crc32.o
#hostprogs-y                      +=bbEnv
#$(srctree)/lib/crc32.o: $(srctree)/lib/crc32.c
#bbEnv:  crc32.o
#	gcc crc32.o -o bbEnv
$(info ************  TEST VERSION ************)
#bbEnv: crc32.o
bbEnv:
	gcc bbEnv.c -I ../../arch/arm/include/string.h -o bbEnv

#crc32.o: ../../lib/crc32.o
#obj-$(CONFIG_CRC32)	+= crc32.o

#crc32.o: ../../lib/crc32.c
#	gcc -c ../../lib/crc32.c 
