//--------------------------------------------------------------------------------------
//
//   satapart command
//
//   Given a partition number, start reading partition into memory
//
//   We only support reading starting at a partition...
//   satapart 0x3008000 partnbr sectcount 
//   arg[0]   arg[1]    arg[2]   arg[3]                
//
//   john c. overton
//   Western Digital Corporation
//
//--------------------------------------------------------------------------------------

#include <common.h>
#include <command.h>
#include <part.h>
#include <sata.h>
#include <gpt.h>

#define CONFIG_SYS_SATA_MAX_DEVICE      1

extern int sata_curr_device;
extern block_dev_desc_t sata_dev_desc[CONFIG_SYS_SATA_MAX_DEVICE];

char buffer[512];
efi_table_hdr_t  gpt_header;



static int do_satapart(struct command *cmdtp, int argc, char *argv[])
{
        int               rc = 0;
        efi_table_hdr_t   gpt_header;
        part_entry*       part;
        int               i, j;
        int               n;
        u32               part_nbr;
        u32               addr;
        u32               sec_count;
        u32               part_count;
        u32               start_sec;

        if ( sata_curr_device == -1) 
        {
            printf("Sata must be initialized first\n");
            return 1;
        }

        // Read the master GPT block. We are assuming this is a good gpt...
        n = sata_read(sata_curr_device, 1, 1, (u32 *) &buffer);

        // Save the header for later...
        memcpy( (void*) &gpt_header, (void *) &buffer, sizeof(gpt_header));

        if( memcmp( &(gpt_header.signature), GPT_SIGNATURE, sizeof(GPT_SIGNATURE)-1) != 0 )
        {
            printf("GPT header not found\n");
            return 1;
        }

        addr      = (int) simple_strtoul( argv[1], NULL, 10);
        part_nbr  = (int) simple_strtoul( argv[2], NULL, 10);
        sec_count = (int) simple_strtoul( argv[3], NULL, 10);

        if ( part_nbr >= (GPT_MAX_PART_ENTRIES/GPT_MAX_PART_PER_BLOCK)  ) 
        {
            printf("Partition number invalid: %d\n", part_nbr);
            return 1;
        }

        // OK, now find the partition we're looking for to find the starting sector #...
        // Assume partition entries are standard size: 128 bytes.
        // Walk thru partition table. 
        // There are # partition entries per blk and # blks allowed for entries.
        //

        part_count = 0;  // Start with partition 1.

        for ( i = 0; i < (GPT_MAX_PART_ENTRIES/GPT_MAX_PART_PER_BLOCK); i++ ) 
        {
            int zero = 0;

            // We start reading from sector 2 and read each time looking for 
            // our partition entry...
            n = sata_read(sata_curr_device, 2+i, 1, (u32 *) buffer);

            part = (part_entry*) buffer;

            for ( j = 0; j < GPT_MAX_PART_PER_BLOCK; j++ ) 
            {
                part_count++;

                if ( part_count == part_nbr ) 
                {
                    if ( memcmp( &(part->part_guid), (int*) &zero, sizeof(zero)) == 0 ) 
                    {
                        printf("Invalid partition number: %d, or partition is deleted\n", part_nbr);
                        return 1;
                    }
                    break;
                }

                part++;
            }

            if ( part_count == part_nbr ) 
                break;
        }

        // OK, we've got the partition entry.  Check to see if starting sector # is non zero..
        start_sec = part->first_lba & 0xffffffff;

        n = sata_read(sata_curr_device, start_sec, sec_count, (u32 *) addr );
        
        if( n != sec_count)
        {
            printf("Error: Incomplete read. Only %d sectors read into address %08x from sector 0x%08x\n", n, addr, start_sec );
            return 1;
        }

        printf("Success: %d sectors read into address %08x from sector 0x%08x\n", n, addr, start_sec );

        return rc;
}

BAREBOX_CMD_START(satapart)
        .cmd            = do_satapart,
        .usage          = "SATA sub system",
BAREBOX_CMD_END

