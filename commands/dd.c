/*
 * (C) Copyright Mindspeed Technologies Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <common.h>
#include <asm/io.h>
#include <command.h>
#include <errno.h>

#define SZ_1K	        0x400
#define SZ_128K         (SZ_1K * 128)
#define ADDR_JUMP_SIZE  SZ_128K

#define PHYS_SDRAM_SIZE (256*1024*1024)
#define DDR_BASEADDR  0x0


static u8 do_wr_rd_transaction(u32 ddr_addr_offset, u64*, u32*, u16 mode);




/******************************************************************************
 * do_wr_rd_transaction
 *
 *****************************************************************************/
static u8 do_wr_rd_transaction(u32 ddr_address_offset, u64 *dword_list, 
				u32 *word, u16 ddr16bit_mode)
{
	u8 j;
	register int reg_0 __asm__ ("r3");
	register int reg_1 __asm__ ("r4");
	u64 *src, *dst;
	u32 read_val;
	u8 ret_val = 0;

	ddr_address_offset &= ~0x3;

	/* Do 64bit wr+rd */
	dst = (u64 *)(DDR_BASEADDR+ddr_address_offset);
	src = dword_list;
	for(j=0; j < 16; j++)
	{
		__asm__ __volatile__ ("ldmia %0, {%1,%2}" \
			: "+r" (src), "=r" (reg_0), "=r" (reg_1) );

		__asm__ __volatile__ ("stmia %0, {%1,%2}" \
			: "+r" (dst), "=r" (reg_0), "=r" (reg_1) );

		if ((*src & 0x000000ff000000ffLL) != (*dst & 0x000000ff000000ffLL))
		{
			ret_val |= 1;
		}
		if ((*src & 0x0000ff000000ff00LL) != (*dst & 0x0000ff000000ff00LL))
		{
			ret_val |= 1 << 1;
		}

		if (!ddr16bit_mode)  {
			if ((*src & 0x00ff000000ff0000LL) != (*dst & 0x00ff000000ff0000LL))
			{
				ret_val |= 1 << 2;
			}
			if ((*src & 0xff000000ff000000LL) != (*dst & 0xff000000ff000000LL))
			{
				ret_val |= 1 << 3;
			}
		}
		*dst = 0; //clear location

		dst++; 
		src++;
	}

	/* Do 32bit wr+rd */
	for (j=0; j < 16; j++)
	{
		*(((volatile u32 *)(DDR_BASEADDR+ddr_address_offset)) + j) = *word;

		read_val = *(((volatile u32 *)(DDR_BASEADDR+ddr_address_offset)) + j);
		if ((read_val & 0x000000FF) != (*word & 0x000000ff))
		{
			ret_val |= 1;
		}
		if ((read_val & 0x0000ff00) != (*word & 0x0000ff00))
		{
			ret_val |= 1 << 1;
		}
		if (!ddr16bit_mode)  {
			if ((read_val & 0x00ff0000) != (*word & 0x00ff0000))
			{
				ret_val |= 1 << 2;
			}
			if ((read_val & 0xff000000) != (*word & 0xff000000))
			{
				ret_val |= 1 << 3;
			}
		}

		*(((volatile u32 *)(DDR_BASEADDR+ddr_address_offset)) + j) = 0; //clear location

		 word++;
	}

	return ret_val;
}

static int do_dd(struct command *cmdtp, int argc, char *argv[])
{

	u32 word_list[16]  = {  0xffffffff, 0x00000000, 0x12345678, 0x9abcdef0,
				0xf7f70202, 0xdfdf2020, 0x80407fbf, 0x08040204,
				0x8080fdfd, 0x0808dfdf, 0xa5a55a5a, 0x5a5aa5a5,
				0xaaaa5555, 0x5555aaaa, 0x0000ffff, 0x0000ffff};

	u64 dword_list[16]  = {  0xffffffff00000000ULL, 0xffffffff00000000ULL, 
				0x1234567876543210ULL, 0x0123456789abcdefULL,
				0xf7f7f7f702020202ULL, 0xdfdfdfdf20202020ULL,
				0x804020107fbfdfefULL, 0x0804020110204080ULL,
				0x80808080fdfdfdfdULL, 0x08080808dfdfdfdfULL,
				0xa5a5a5a55a5a5a5aULL, 0x5a5a5a5aa5a5a5a5ULL,
				0xaaaaaaaa55555555ULL, 0x55555555aaaaaaaaULL,
				0x00000000ffffffffULL, 0x00000000ffffffffULL
			};

	u8 result = 0;
	u32 ddr_addr_offset = 0;

	while(1) {

		result = do_wr_rd_transaction(ddr_addr_offset,dword_list,word_list, 0);

		if (result)
			putchar('.');  //Fail
		else
			putchar('#');  //Pass

		ddr_addr_offset = (ddr_addr_offset + ADDR_JUMP_SIZE) & (PHYS_SDRAM_SIZE -1);

	}

	return 0;
}



BAREBOX_CMD_HELP_START(dd)
BAREBOX_CMD_HELP_USAGE("ddr diag\n")
BAREBOX_CMD_HELP_SHORT("Run DDR diags:\n")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(dd)
    .cmd        = do_dd,
    .usage      = "DDR test",
    BAREBOX_CMD_HELP(cmd_dd_help)
BAREBOX_CMD_END

