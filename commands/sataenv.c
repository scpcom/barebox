//--------------------------------------------------------------------------------------
//
//   sataenv command
//
//   Given a partition number, assume it's an environment that needs to be loaded
//
//   We only support reading inn an environment from a sata partition...
//   sataenv  load      partnbr  
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
#include <malloc.h>
#include <errno.h>
#include <fs.h>
#include <fcntl.h>
#include <envfs.h>
#include <xfuncs.h>
#include <libbb.h>
#include <libgen.h>
#include <environment.h>


#define CONFIG_SYS_SATA_MAX_DEVICE      1
#define DEFAULT_ENVIRONMENT_DIRECTORY   "/env"

#define BLOCK_SIZE 512

extern int sata_curr_device;
extern block_dev_desc_t sata_dev_desc[CONFIG_SYS_SATA_MAX_DEVICE];

static char temp_buffer[BLOCK_SIZE];



static u32 find_part_start( char* prog_name, int part_nbr )
{
    efi_table_hdr_t   gpt_header;
    part_entry*       part;
    int               i, j;
    int               n;
    u32               part_count;
    u32               start_sec;


    // Read the master GPT block. We are assuming this is a good gpt...
    n = sata_read(sata_curr_device, 1, 1, (u32 *) &temp_buffer);

    // Save the header for later...
    memcpy( (void*) &gpt_header, (void *) &temp_buffer, sizeof(gpt_header));

    if( memcmp( &(gpt_header.signature), GPT_SIGNATURE, sizeof(GPT_SIGNATURE)-1) != 0 )
    {
        printf("%s: GPT header not found\n", prog_name);
        return 1;
    }

    if ( part_nbr >= (GPT_MAX_PART_ENTRIES/GPT_MAX_PART_PER_BLOCK)  ) 
    {
        printf("%s: Partition number invalid: %d\n", prog_name, part_nbr);
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
        n = sata_read(sata_curr_device, 2+i, 1, (u32 *) temp_buffer);

        part = (part_entry*) temp_buffer;

        for ( j = 0; j < GPT_MAX_PART_PER_BLOCK; j++ ) 
        {
            part_count++;

            if ( part_count == part_nbr ) 
            {
                if ( memcmp( &(part->part_guid), (int*) &zero, sizeof(zero)) == 0 ) 
                {
                    printf("%s: Invalid partition number: %d, or partition is deleted\n", prog_name, part_nbr);
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

    return start_sec;
}


static int sata_load_environment(char* prog_name, u32 part_nbr, u32 start_sec, char* dir)
{
    int                 n;
    struct envfs_super  super;
    u32                 size;
    u32                 alloc_size;
    char*               buf;
    char*               buf_free;
    char*               str;
    char*               tmp;
    int                 namelen_full;
    int                 fd;


    // ok, we have start of partitiont.  We will read in the first sector 
    // and see if there is an environment fs superblock...
    n = sata_read(sata_curr_device, start_sec, 1, (u32*) temp_buffer );

    memcpy( (void*) &super, (void*) &temp_buffer, sizeof(super) );

    if( ENVFS_32(super.magic) != ENVFS_MAGIC )
    {
        printf("%s: partition %d not an environment\n", prog_name, part_nbr);
        return 1;
    }

    if (crc32(0, (unsigned char*) &super, sizeof(struct envfs_super) - 4) != ENVFS_32(super.sb_crc)) 
    {
        printf("%s: partition %d wrong crc on env superblock\n", prog_name, part_nbr);
        return 1;
    }

    // OK, super must be good, so read in the whole environment...
    size = ENVFS_32(super.size);

    // Allocate memory for all sectors (super and data), buf round up to next sector size.
    // We are re-reading the superblock just because...
    alloc_size = ((size + sizeof(super) + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    buf = xmalloc(alloc_size);
    buf_free = buf;

    n = sata_read( sata_curr_device, start_sec, alloc_size / BLOCK_SIZE, (u32*) buf);

    buf += sizeof(super);           // Bump up buffer pointer to start of first inode...

    // Check data...
    if (crc32( 0, (unsigned char*) buf, size) != ENVFS_32(super.crc)) 
    {
        printf("%s: partition %d wrong crc on env data\n", prog_name, part_nbr);
        return 1;
    }

    while (size) 
    {
        int                  rc;
        struct envfs_inode*  inode;
        u32                  inode_size;
        u32                  inode_namelen;

        inode = (struct envfs_inode*) buf;

        if ( ENVFS_32(inode->magic) != ENVFS_INODE_MAGIC) 
        {
            printf("%s: partition %d wrong magic on inode\n", prog_name, part_nbr);
            return 1;
        }

        inode_size    = ENVFS_32(inode->size);
        inode_namelen = ENVFS_32(inode->namelen);

        
        printf("%s: partition %d loading %s size %d\n", prog_name, part_nbr, inode->data, inode_size);

        str = concat_path_file(dir, inode->data);
        tmp = strdup(str);
        make_directory(dirname(tmp));
        free(tmp);

        fd = open(str, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) 
        {
            printf("%s: partition %d file: %s, %s\n", prog_name, part_nbr, str, errno_str());
            free(str);
            return 1;
        }

        free(str);

        namelen_full = PAD4(inode_namelen);

        rc = write( fd, buf + namelen_full + sizeof(struct envfs_inode), inode_size);

        if (rc < inode_size) 
        {
            perror("write");
            rc = errno;
            close(fd);
            free(buf_free);
            return rc;
        }

        close(fd);

        buf  += PAD4(inode_namelen) +
                PAD4(inode_size) +
                sizeof(struct envfs_inode);

        size -= PAD4(inode_namelen) +
                PAD4(inode_size) +
                sizeof(struct envfs_inode);

    }

    free(buf_free);

    return 0;
}


static int sata_run_env_command(char* prog_name, u32 part_nbr, u32 start_sec, char* script_name )
{
    int                 n;
    struct envfs_super  super;
    u32                 size;
    u32                 alloc_size;
    char*               buf;
    char*               buf_free;
    char*               str;
    char*               tmp;
    int                 namelen_full;
    int                 fd;


    // ok, we have start of partitiont.  We will read in the first sector 
    // and see if there is an environment fs superblock...
    n = sata_read(sata_curr_device, start_sec, 1, (u32*) temp_buffer );

    memcpy( (void*) &super, (void*) &temp_buffer, sizeof(super) );

    if( ENVFS_32(super.magic) != ENVFS_MAGIC )
    {
        printf("%s: partition %d not an environment\n", prog_name, part_nbr);
        return 1;
    }

    if (crc32(0, (unsigned char*) &super, sizeof(struct envfs_super) - 4) != ENVFS_32(super.sb_crc)) 
    {
        printf("%s: partition %d wrong crc on env superblock\n", prog_name, part_nbr);
        return 1;
    }

    // OK, super must be good, so read in the whole environment...
    size = ENVFS_32(super.size);

    // Allocate memory for all sectors (super and data), buf round up to next sector size.
    // We are re-reading the superblock just because...
    alloc_size = ((size + sizeof(super) + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    buf = xmalloc(alloc_size);
    buf_free = buf;

    n = sata_read( sata_curr_device, start_sec, alloc_size / BLOCK_SIZE, (u32*) buf);

    buf += sizeof(super);           // Bump up buffer pointer to start of first inode...

    // Check data...
    if (crc32( 0, (unsigned char*) buf, size) != ENVFS_32(super.crc)) 
    {
        printf("%s: partition %d wrong crc on env data\n", prog_name, part_nbr);
        return 1;
    }

    // Run the first file we come across...
    while (size) 
    {
        int                  rc;
        struct envfs_inode*  inode;
        u32                  inode_size;
        u32                  inode_namelen;
        char*                script;

        inode = (struct envfs_inode*) buf;

        if ( ENVFS_32(inode->magic) != ENVFS_INODE_MAGIC) 
        {
            printf("%s: partition %d wrong magic on inode\n", prog_name, part_nbr);
            return 1;
        }

        inode_size    = ENVFS_32(inode->size);
        inode_namelen = ENVFS_32(inode->namelen);

        
        printf("%s: partition %d loading %s size %d\n", prog_name, part_nbr, inode->data, inode_size);

        //str = concat_path_file(dir, inode->data);
        //tmp = strdup(str);
        //make_directory(dirname(tmp));
        //free(tmp);

        //free(str);

        namelen_full = PAD4(inode_namelen);

        script = xzalloc(inode_size + 1);

        memcpy(script, buf + namelen_full + sizeof(struct envfs_inode), inode_size);

        rc = run_command( script, 0);

        free( script );

        break;

        buf  += PAD4(inode_namelen) +
                PAD4(inode_size) +
                sizeof(struct envfs_inode);

        size -= PAD4(inode_namelen) +
                PAD4(inode_size) +
                sizeof(struct envfs_inode);

    }

    free(buf_free);

    return 0;
}



static int do_sataenv(struct command *cmdtp, int argc, char *argv[])
{
    int               rc = 0;
    u32               part_nbr;
    u32               start_sec;

    if ( sata_curr_device == -1) 
    {
        printf("%s: Sata must be initialized first\n", argv[0]);
        return 1;
    }

    // Must have three args...
    //if (argc != 3) 
    //{
    //    printf("%s: Must have two args\n\n", argv[0]);
    //    printf("Usage:\n");
    //    printf("%s load partnbr\n", argv[0]);
    //    return 1;
    //}

    part_nbr = (int) simple_strtoul( argv[2], NULL, 10);

    if (strncmp(argv[1], "load", 4) == 0) 
    {
        // First, we have to find the start of the partition...
        // If there was an error, find_part_start() has already
        // shown error message...
        if ( (start_sec = find_part_start( argv[0], part_nbr)) < 0) 
        {
            return 1;
        }

        rc = sata_load_environment(argv[0], part_nbr, start_sec, DEFAULT_ENVIRONMENT_DIRECTORY);
    }
    else if (strncmp(argv[1], "run", 3) == 0) 
    {
        // First, we have to find the start of the partition...
        // If there was an error, find_part_start() has already
        // shown error message...
        if ( (start_sec = find_part_start( argv[0], part_nbr)) < 0) 
        {
            return 1;
        }

        rc = sata_run_env_command(argv[0], part_nbr, start_sec, argv[3] );
    }
    else
    {
        printf("%s: invalid argument\n", argv[0]);
        return 1;
    }

    return rc;
}

BAREBOX_CMD_START(sataenv)
        .cmd            = do_sataenv,
        .usage          = "SATA sub system",
BAREBOX_CMD_END

