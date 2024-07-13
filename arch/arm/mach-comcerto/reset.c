#include <common.h>
#include <init.h>
#include <asm/io.h>
#include <mach/comcerto-2000.h>
#include <mach/a9_mpu.h>

/*
 *  * Reset the cpu through the reset controller
 *   */
void __noreturn reset_cpu (unsigned long addr)
{
	writel( 0xff, COMCERTO_APB_CLK_BASE);
//	writel(0xf,  COMCERTO_A9_TIMER_BASE + A9_WD_COUNTER);
//	writel(A9_WD_ENABLE| A9_WD_MODE_WD,  COMCERTO_A9_TIMER_BASE + A9_WD_COUNTER);

//        while (1);
}
EXPORT_SYMBOL(reset_cpu);

