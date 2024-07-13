


#ifndef	_GPT_H
#define	_GPT_H	1


#define GPT_MAX_PART_ENTRIES       128
#define GPT_MAX_PART_PER_BLOCK     4
#define GPT_SIGNATURE              "EFI PART"

/*
 * Generic EFI table header
 */
typedef	struct 
{
	u64  signature;
	u32  revision;        // in big-endian.
	u32  headersize;
	u32  crc32;
	u32  reserved;
    u64  current_lba; 
    u64  backup_lba;
    u64  first_usable_lba;
    u64  last_usable_lba;
    char disk_guid[16];
    u64  part_table_starting_lba;
    u32  part_entry_count;
    u32  part_entry_size;
    u32  crc32_part_table;

} efi_table_hdr_t;



typedef struct
{
    char part_guid[16];
    char uniq_guid[16];
    u64  first_lba;
    u64  last_lba;
    u64  attr_flags;
    char part_name[72];
} part_entry;




#endif /* gpt.h  */
