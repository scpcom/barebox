#include <common.h>
#include <driver.h>
#include <init.h>
#include <asm/io.h>
#include <partition.h>
#include <mach/comcerto-2000.h>
#include <config.h>

struct device_d c2k_nor_dev = {
	.id	  = -1,
	.name     = "cfi_flash",
	.map_base = COMCERTO_AXI_EXP_BASE,
	.size     = NOR_FLASH_SIZE,
};

static int c2k_nor_init(void)
{
	
	register_device(&c2k_nor_dev);

/*
 * Warning, please make sure to synchronize with the nor_parts environement variable. 
 */
	devfs_add_partition("nor0", 0x60000, 0x20000, PARTITION_FIXED, "env0");
	protect_file("/dev/env0", 1);

	return 0;
}

device_initcall(c2k_nor_init);

