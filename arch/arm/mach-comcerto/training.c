#include <common.h>
#include <init.h>
#include <asm/io.h>
#include <asm/types.h>
#include <mach/ddr.h>

/* enable following define to print out DDR Training result to bootloader console (requires UART to be up and running) */
//#define DDR_TRAIN_INFO

#define NUM_BYTES	4 //ECC bytes which is the 5th bytes is not supported in the training

typedef struct offset_win_s
{
  int win_start;
  int win_end;
  int win;

  int gWin;
  int gWin_start;
  int gWin_end;
  
  int w_dq;
  int r_dqs;
}offset_win_t;

offset_win_t ctrl_offset[NUM_BYTES];

u32 crtl_lock_val;
int crtl_offset_max;

#define DDR_TRAIN_LOC	0x0

/* From ddr training hw specs v0.3 
*
* Initiate 64bits read and write operations to 8 differents addresses (offset 0x0, 0x8, 0x10, 0x18...)
* Initiate 32bit single operations (write and reads) to 8 different addressese (offset 0x0, 0x4, 0x8, 0xC...)
*/
#define DDR_TRAIN_NUM_OFFSET	8

#define NUM_32BIT_PATTERN	4
#define NUM_64BIT_PATTERN	9

#define CTRL_LOCK_SHIFT	4
#define CTRL_LOCK_MASK	0x3ff

static u8 do_wr_rd_test(u64 *pattern64bit, u32 *pattern32bit, u16 ddr16bit_mode);
static void update_window(u8 result, int delay_offset);
static void configure_offset(int write_side);
void arm_write64(u64 data,volatile u64 *p);

static inline void enable_resync(void)
{
	u32 ctrl_val;
        //Enable PHY ctrl_resync
        ctrl_val = readl(DDR_PHY_CTL_07_REG);
        ctrl_val |= CTRL_RESYNC_EN;
        writel(ctrl_val, DDR_PHY_CTL_07_REG);
}


static inline void disable_resync(void)
{
	u32 ctrl_val;
        //Clear PHY ctrl_resync
        ctrl_val = readl(DDR_PHY_CTL_07_REG);
        ctrl_val &= ~CTRL_RESYNC_EN;
        writel(ctrl_val, DDR_PHY_CTL_07_REG);
}

static inline void assert_resync(void)
{
	u32 ctrl_val;
	//Assert ctrl_resync pulse 
	ctrl_val = readl(DDR_PHY_CTL_07_REG);
	ctrl_val |= CTRL_RESYNC_PLS;
	writel(ctrl_val, DDR_PHY_CTL_07_REG); 
}

/* Write to the Control Register 4 and 5 to set the delay offset to 
*  write DQ values for each byte
*/
static void write_dq_offset_reg(u8 byte0, u8 byte1, u8 byte2, u8 byte3)
{
	u32 ctrl4_val;
	u32 ctrl5_val;
	u32 ctrl_val;
	int i;
	u8 byte[3];

	byte[0] = byte0;
	byte[1] = byte1;
	byte[2] = byte2;

	ctrl4_val = readl(DDR_PHY_CTL_04_REG);
	ctrl_val = ctrl4_val & 0x7f;

	for(i = 0 ; i < 3; i++)
	{
		 if(byte[i] < 0)
		 {
			byte[i] = -byte[i];
			ctrl_val |= 0x1 << ((i+1) * 8 + 6);
		 }
		 ctrl_val |= (byte[i] << ((i+1) * 8));
	}

	//Write the first 3 byte in Control Register 4
	writel(ctrl_val, DDR_PHY_CTL_04_REG);
	
	ctrl5_val = readl(DDR_PHY_CTL_05_REG);
	ctrl_val = ctrl5_val & ~0x7f;

	if(byte3 < 0)
	{
		byte3 = -byte3;
		ctrl_val |= 0x1 << 6;
	}
	ctrl_val |= byte3 ;
	
	//Write the 4th byte in Control Register 5
	writel(ctrl_val, DDR_PHY_CTL_05_REG);

	assert_resync();
}

/* Write to the Control Register 3 to set the delay offset to 
*  read DQS values for each byte
*/
static void read_dqs_offset_reg(u8 byte0, u8 byte1, u8 byte2, u8 byte3)
{
	int i;
	u8 byte[4];
	u32 ctrl_val = 0;

	byte[0] = byte0;
	byte[1] = byte1;
	byte[2] = byte2;
	byte[3] = byte3;

        for(i = 0 ; i < NUM_BYTES; i++)
        {
                 if(byte[i] < 0)
                 {
                        byte[i] = -byte[i];
                        ctrl_val |= 0x1 << (i * 8 + 6);
                 }
                 ctrl_val |= byte[i] << (i * 8);
        }

	writel(ctrl_val, DDR_PHY_CTL_03_REG);

	assert_resync();
}

static void init_windows_pointers(void)
{
	int i;

	for(i = 0 ; i < NUM_BYTES; i++)
	{
		ctrl_offset[i].win_start = 0;
		ctrl_offset[i].win_end = 0;
		ctrl_offset[i].win = 0;
		ctrl_offset[i].gWin = 0;
	}
}


void ddr_training(void)
{

	u32 pattern32_list[NUM_32BIT_PATTERN] = {
			0xffffffff,
			0x00000000,
			0xaaaaaaaa,
			0x55555555
			};

	u64 pattern64_list[NUM_64BIT_PATTERN] = {
			0xffffffffffffffffULL,
			0x0000000000000000ULL,
			0x55555555aaaaaaaaULL,
			0xaaaaaaaa55555555ULL,
			0xdfeff7fb20100804ULL,
			0x04081020fbf7efdfULL,
			0xffffffff00000000ULL,
			0x00000000ffffffffULL,
			0x12345678fedcba98ULL
			};

	u8 res;
	int i;

	crtl_lock_val = (readl (DDR_PHY_DLL_STAT_REG) >> CTRL_LOCK_SHIFT ) & CTRL_LOCK_MASK ;
	crtl_offset_max = crtl_lock_val/2;

	//<----------- Write Side Training ----------->//

	init_windows_pointers();

	enable_resync();

	//Move from negative min offset to positive max offset
	for(i = -crtl_offset_max ; i <= crtl_offset_max; i++)
	{
		write_dq_offset_reg(i, i, i, i);

		res = do_wr_rd_test(pattern64_list, pattern32_list, 0);

		update_window(res, i);	
	}

	configure_offset(1);	
	
	disable_resync();

	//<----------- Read Side Training ----------->//
	
	init_windows_pointers();

	enable_resync();
	
        //Move from negative offset to positive offset
        for(i = -crtl_offset_max ; i <= crtl_offset_max; i++)
        {
                read_dqs_offset_reg(i, i, i, i);

                res = do_wr_rd_test(pattern64_list, pattern32_list, 0);

                update_window(res, i);
        }

        configure_offset(0);

	//TBD:In case window size is too small then print an error message and allow to use UART for debugging 
}

static u8 do_wr_rd_test(u64 *pattern64bit, u32 *pattern32bit, u16 ddr16bit_mode)
{
	int i;
	volatile u64 *src64, *dst64;
	volatile u32 *src32, *dst32;
	volatile u32 read_val = 0;
	u8 ret_val = 0;
	u8 dst_num_offset;

	dst64 = (u64 *)(DDR_TRAIN_LOC);
	src64 = pattern64bit;
	
	for (dst_num_offset = 0; dst_num_offset < DDR_TRAIN_NUM_OFFSET; dst_num_offset++)
	{
		for(i = 0 ; i < NUM_64BIT_PATTERN ; i++)
		{
			arm_write64(pattern64bit[i], (volatile u64 *) dst64);

			if ((*src64 & 0x000000ff000000ffLL) != (*dst64 & 0x000000ff000000ffLL))
			{
				ret_val |= 1;
			}
			if ((*src64 & 0x0000ff000000ff00LL) != (*dst64 & 0x0000ff000000ff00LL))
			{
				ret_val |= 1 << 1;
			}

			if (!ddr16bit_mode)  {
				if ((*src64 & 0x00ff000000ff0000LL) != (*dst64 & 0x00ff000000ff0000LL))
				{
					ret_val |= 1 << 2;
				}
				if ((*src64 & 0xff000000ff000000LL) != (*dst64 & 0xff000000ff000000LL))
				{
					ret_val |= 1 << 3;
				}
			}
			*dst64 = 0x0; //clear location

			//do next pattern still at same address
			src64++;
		}
		
		//restart all patterns at different address
		src64 = pattern64bit;
		dst64++;
	}

        dst32 = (u32 *)(DDR_TRAIN_LOC);
        src32 = pattern32bit;

	for (dst_num_offset = 0; dst_num_offset < DDR_TRAIN_NUM_OFFSET; dst_num_offset++)
	{
		for(i = 0 ; i < NUM_32BIT_PATTERN ; i++)
		{
			writel(pattern32bit[i], (volatile u32 *) dst32);
			read_val = readl(dst32);

			if ((read_val & 0x000000ff) != (*src32 & 0x000000ff))
			{
				ret_val |= 1;
			}
			if ((read_val & 0x0000ff00) != (*src32 & 0x0000ff00))
			{
				ret_val |= 1 << 1;
			}
			if (!ddr16bit_mode)  {
				if ((read_val & 0x00ff0000) != (*src32 & 0x00ff0000))
				{
					ret_val |= 1 << 2;
				}
				if ((read_val & 0xff000000) != (*src32 & 0xff000000))
				{
					ret_val |= 1 << 3;
				}

				*dst32 = 0x0;
			}
			//do next pattern still at same address
			src32++;
		}
		
		//restart all patterns at different address
		src32 = pattern32bit;
		dst32++;
	}
	return ret_val;
}

static void update_window(u8 result, int delay_offset)
{
	int i;
	u8 fail;

	for(i = 0; i < NUM_BYTES; i++)
	{
		fail = (result >> i) & 0x1;

		if(!fail) //pass
		{
			/* its first time success, start counting from here */
			if (ctrl_offset[i].win_start == 0)
			{
				ctrl_offset[i].win_start = ctrl_offset[i].win_end = delay_offset;
			}
			else  
			{
				ctrl_offset[i].win_end = delay_offset; 
			}						
		}
		else //failure
		{			
			if (ctrl_offset[i].win_start == 0)	
			{//window not yet started, so ignore this delay offset value for this byte		
				continue;
			}
			else	//this window ends here,calculate window size
			{
				ctrl_offset[i].win = ctrl_offset[i].win_end - ctrl_offset[i].win_start;
			}
		}
	
		if(ctrl_offset[i].gWin < ctrl_offset[i].win)
		{
			ctrl_offset[i].gWin = ctrl_offset[i].win;
			ctrl_offset[i].gWin_start = ctrl_offset[i].win_start;
			ctrl_offset[i].gWin_end = ctrl_offset[i].win_end;
		}
	}
}

static void configure_offset(int if_write_side)
{
	int i;
	int win_start;
	int win_end;
	int delay_offset;

	for(i = 0; i < NUM_BYTES; i++)
        {
		win_start = ctrl_offset[i].gWin_start;
		win_end = ctrl_offset[i].gWin_end;

		delay_offset = (win_start + win_end)>> 1;
	
		if(if_write_side)
			ctrl_offset[i].w_dq = delay_offset;
		else
			ctrl_offset[i].r_dqs = delay_offset;
	}		

	if(if_write_side == 1)	{	
		write_dq_offset_reg(ctrl_offset[0].w_dq, ctrl_offset[1].w_dq, ctrl_offset[2].w_dq, ctrl_offset[3].w_dq);
	//#ifdef DDR_TRAIN_INFO
	//	printf("DDR Training: Write DQ: %x %x %x %x\n", byte[0], byte[1],byte[2],byte[3]);
	//#endif		
	}
	else {
		read_dqs_offset_reg(ctrl_offset[0].r_dqs, ctrl_offset[1].r_dqs, ctrl_offset[2].r_dqs, ctrl_offset[3].r_dqs);
	//#ifdef DDR_TRAIN_INFO
	//	printf("DDR Training: READ DQS: %x %x %x %x\n", byte[0], byte[1],byte[2],byte[3]);
	//#endif	
	}
}

void arm_write64(u64 data,volatile u64 *p)
{
        int *tptr = (int*)&data;
        register int reg_0 __asm__ ("r3");
        register int reg_1 __asm__ ("r4");

        __asm__ __volatile__ (
                "ldmia     %0, {%1,%2}     \n\t"
                : "+r" (tptr), "=r" (reg_0), "=r" (reg_1)
        );
        __asm__ __volatile__ (
                "stmia     %0, {%1,%2}     \n\t"
                : "+r" (p), "=r" (reg_0), "=r" (reg_1)
        );
}

int get_ddr_training_result(u8 *w_dq, u8 *r_dqs)
{
	int i;
	int result = 0;

	for(i = 0; i < NUM_BYTES; i++)
	{
		//if(ctrl_offset[i].gWin < MIN_WINDOW_SIZE)
		//	result = -1;
			
		w_dq[i] = ctrl_offset[i].w_dq;
		r_dqs[i] = ctrl_offset[i].r_dqs;
	}

	return result;
}
