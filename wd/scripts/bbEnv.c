#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define __BYTE_ORDER __LITTLE_ENDIAN
#else
#define __BYTE_ORDER __BIG_ENDIAN
#endif

typedef unsigned int uint32_t;
#include "../../lib/crc32.c"
#include "../../include/envfs.h"	// defines struct envfs_inode, envfs_super


uint32_t crc32(uint32_t crc, const void *_buf, unsigned int len);


#define BB_ENV_CONFIG_FILE "/config"
#define FLASH_WRITE_RETRY_MAX  5

#define PAD4(x) ((x + 3) & ~3)


/* Parsed out variables... */
struct var {
    struct var*  next;
    char*  var_name;
    char*  var_value;
};



#define TOKEN_COMMENT   '#'
#define TOKEN_EQUAL     '='
#define TOKEN_TERMS     " ;,=%&#@!*\r\n"

/*
 *  advance_line() -- advance scan pointer to next line. 
 */
 char* gobble_whitespace(char* s)
 {
     /* Ok, now gobble up whitespace to get to start of next token... */
     while( *s == ' ' || *s == '\t' ) {
         s++;
     }
     return s;
 }



/*
 *  advance_line() -- advance scan pointer to next line. 
 */
char* advance_line(char* s)
{
    while(*s != 0x00 && *s != 0x0a && *s != ';') 
        s++;

    if(*s == 0x0a || *s == ';') 
        s++;

    return s;
}



/*
 *  dup_token() -- duplicate token pointed to by s...
 */
char* dup_token(char* s)
{
    char* t = s;
    int   l;
    char* p = NULL;

    /* Assume that s points to start of token. Find end of token... */
    t = strpbrk( s, TOKEN_TERMS );
    if( t != NULL) {
        l = t - s;
        p = calloc(l + 1, 1);
        memcpy(p, s, l);
        p[l] = 0;
    } else {
        l = strlen(s);
        p = calloc(l + 1, 1);
        strcpy(p, s);
    }
    
    return p;
}



/*
 *  dup_token() -- duplicate rest of line pointed to by s...
 */
char* dup_rest_of_line(char* s)
{
    char* t;
    int   l;
    char* p = NULL;

    /* Scan to end of line... */
    t = s;
    while(*t != 0x00 && *t != 0x0a && *t != ';') 
        t++;
    l = t - s;
    p = calloc(l + 1, 1);
    memcpy(p, s, l);
    p[l] = 0;
    
    return p;
}



/*
 *  advance_to_token() -- advance scan pointer to start of next token.  Assume
 *                        we want the next one and not the one we be sitting on... 
 */
char* advance_to_token(char* s)
{
    char*  t;

    /* First, check to make sure we're not at end of line... */
    if ( *s == 0x00 || *s == 0x0a ) 
        return s;

    /* Next, we want to advance over current token... */
    t = strpbrk( s, TOKEN_TERMS );      /* Find end of token.         */
    if( s == t ) {                      /* Was sitting on a TERM.     */
        s++;                            /* Advance past TERM.         */
    } else {
        s = t;                          /* Beyond previous token.     */
    }

    s = gobble_whitespace(s);           /* Advance over whitespace.   */

    return s;
}



/*
 *  parse_variables() -- Parse out variables from config file, which has been read into
 *                       into memory and is passed to us in a pointer...
 */
int   parse_variables(char* config, struct var** vars)
{
    char* s = config;
    char* save_vn;
    struct var* var;

    if ( config == NULL) {
        return -1;
    }

    /*  Parse out a line at a time... */
    while (*s != 0x00 ) {

        if ( *s == TOKEN_COMMENT ) {
            s = advance_line(s);
            continue;
        }

        save_vn = s;  /* save start of potential variable */

        s = advance_to_token(s);

        if ( *s == TOKEN_EQUAL ) {
            s = advance_to_token(s);
            var = calloc(sizeof(struct var), 1);
            var->var_name  = dup_token(save_vn);
            var->var_value = dup_rest_of_line(s);
            var->next      = *vars;
            *vars          = var;
        }

        s = advance_line(s);
    }

    return 0;
}




/*
 * envfs_find_config() -- Read in barebox environment and return config in
 *                        a string...
 */

int envfs_find_config(char *filename, char** environment, int* env_length, char** config )
{
    struct envfs_super super;
    char* buf       = NULL;
    char* buf_start;
    char* buf_end;
    int envfd;
    int fd; 
    int ret = -1;
    char* str;
    char* tmp;
    int namelen_full;
    unsigned long size;
    struct envfs_inode *save_config_inode = NULL;
    struct envfs_inode *inode;
    uint32_t inode_size;
    uint32_t inode_namelen;
    int      length;


    *config = NULL;

    envfd = open(filename, O_RDONLY);
    if (envfd < 0) {
        fprintf(stderr, "Open %s %d\n", filename, errno);
        return -1;
    }

    /* read superblock */
    ret = read(envfd, &super, sizeof(struct envfs_super));
    if ( ret < sizeof(struct envfs_super)) {
        perror("read");
        ret = errno;
        goto out;
    }

    if ( ENVFS_32(super.magic) != ENVFS_MAGIC) {
        fprintf(stderr, "envfs: wrong magic on %s\n", filename);
        ret = -1;
        goto out;
    }

    if (crc32(0, (unsigned char *)&super, sizeof(struct envfs_super) - 4)
           != ENVFS_32(super.sb_crc)) {
        fprintf(stderr, "wrong crc on env superblock\n");
        ret = -1;
        goto out;
    }

    size           = ENVFS_32(super.size);
    buf            = calloc(size + sizeof(struct envfs_super), 1);
    (*environment) = calloc(size + sizeof(struct envfs_super), 1);
    buf_start      = buf;
    buf_end        = buf + size + sizeof(struct envfs_super);

    memcpy(buf, (void*) &super, sizeof(struct envfs_super));
    buf += sizeof(struct envfs_super);

    ret = read(envfd, buf, size);
    if (ret < size) {
        perror("read");
        ret = errno;
        goto out;
    }

    if (crc32(0, (unsigned char *)buf, size)
             != ENVFS_32(super.crc)) {
        fprintf(stderr, "wrong crc on env\n");
        ret = -1;
        goto out;
    }

    /*
     * Ok, now find config. Go thru the whole environment looking for it.  When we find it,
     * we'll save a pointer to it's inode and then break out...
     */

    while (size) {

        
        inode = (struct envfs_inode *)buf;

        if (ENVFS_32(inode->magic) != ENVFS_INODE_MAGIC) {
            fprintf(stderr, "envfs: wrong magic on %s\n", filename);
            ret = -1;
            goto out;
        }

        inode_size    = ENVFS_32(inode->size);
        inode_namelen = ENVFS_32(inode->namelen);

        if ( strcmp(inode->data, BB_ENV_CONFIG_FILE) == 0 ) {
            save_config_inode = inode;      /* Save config's inode.*/
            break;                          /* and we are done looking for config. */
        }

        namelen_full = PAD4(inode_namelen+1);

        buf += PAD4(inode_namelen) + PAD4(inode_size) +
                sizeof(struct envfs_inode);
        size -= PAD4(inode_namelen) + PAD4(inode_size) +
                sizeof(struct envfs_inode);
    }

    /* If we found config, allocate storage for it and copy it to return to caller.  */
    if ( save_config_inode != NULL  ) {
        (*config) = calloc(inode_size+1, 1);
        memcpy( *config, buf + PAD4(inode_namelen) + sizeof(struct envfs_inode), inode_size);
        (*config)[inode_size] = '\0';

        /* Now, we will copy the environment excluding the config... */
        length = buf - buf_start;
        memcpy( *environment, buf_start, length);  /* Copy upto config */
        tmp    = (*environment) + length;          

        /* Bump over config...  */
        buf += PAD4(inode_namelen) + PAD4(inode_size) +
                sizeof(struct envfs_inode);

        /* Calculate length and copy the remainder */
        length = buf_end - buf;
        memcpy( tmp, buf, length);  /* Copy after config to end. */
        tmp += length;
        tmp[0] = 0;                 /* null terminate. */

        (*env_length) = tmp - (*environment);

        ret = 0;
    } else {
        fprintf(stderr, "Did not find config file!\n");
    }


out:
    close(envfd);

    if (buf_start)
        free(buf_start);
	
    return ret;
}




/*
 *  Print all variables...
 */
int print_variables(struct var* vars)
{
    while (vars) 
    {
        printf("%s = %s\n", vars->var_name, vars->var_value);
        vars = vars->next;
    } 

    return 0;
}

/*
 *  Locate a specific variable and print its value...
 */

int print_var_value(struct var* vars, char* variable)
{
    while (vars) 
    {
        if ( strcmp(vars->var_name, variable) == 0) {
            printf("%s\n", vars->var_value);
            return 0;
        }
        vars = vars->next;
    } 

    return 0;
}




/*
 *  Free memory for any/all variables...
 */
int free_variables(struct var** vars)
{
    struct var* free_var;

    while (*vars) 
    {
        free_var = *vars;
        *vars = (*vars)->next;
        free(free_var->var_name);
        free(free_var->var_value);
        free(free_var);
    } 

    return 0;
}



int parse_and_replace_vars(struct var** vars, struct var** new_vars)
{
    struct var* var;
    struct var* scan_var;


    if ( *vars == NULL ) {
        fprintf(stderr, "No vars passed on command line.\n");
        return -1;
    }

    /* Now, replace any existing vars... */
    while ( *new_vars) {

        var      = *new_vars;
        *new_vars = var->next;
        scan_var = *vars;
        while(scan_var) {

            /* See if there is same var in existing vars. If so, replace... */
            if ( strcmp(scan_var->var_name, var->var_name) == 0 ) {
                free(scan_var->var_value);  /* Free old value */
                scan_var->var_value = var->var_value;
                free(var->var_name);
                free(var);
                var = NULL;
                break;
            }
            scan_var = scan_var->next;
        }

        /* Did we find and update an existing one? If not, then... */
        if ( var != NULL ) {
            /* Add new var onto chain of old vars... */
            var->next = *vars;
            (*vars) = var;
        }
    } 

    return 0;
}


/*
 *  Rebuild config file with new variables. We save the first couple of comment lines, then
 *  put in all variable assignments.  We will free and re-allocate config...
 */
int  add_back_vars_to_config(struct var** vars, char** config)
{
    char* new_config;
    char* s = *config;
    int   length = 0;
    struct var* var;

    /* First, calculate memory size of all variables needed... */
    var = *vars;
    while (var) {
        length += strlen(var->var_name);
        length += strlen(var->var_value);
        length += 2;   /* Plus = and cr */
        var = var->next;
    }

    /* Now, figure out how much of the beginning of the old config we'll keep... */
    while( *s != 0x00 ) {

        if (*s != TOKEN_COMMENT) {
            break;
        }

        s = advance_line(s);
    }

    new_config = calloc(length + (s - (*config)) + 10, 1);

    length = s - (*config);

    memcpy(new_config, *config, length);

    s = new_config + length;

    *s++ = 0x0a;

    var = *vars;
    while (var) {
        int i;
        i = sprintf(s, "%s=%s\n", var->var_name, var->var_value);
        s += i;                /* bump up */
        var = var->next;
    }

    free(*config);
    (*config) = new_config;

    return 0;
}



int envfs_save( char* new_file, char** environment, int env_length, char* config)
{
    struct envfs_super *super;
    int envfd, size, ret;
    void* buf = NULL;
    char* new_env;
    int   new_len;
    int   config_len = strlen(config);
    struct envfs_inode *inode;
    int    name_len;


    /* We are going to rebuild environment, sticking in config as first file... */
    name_len       = strlen( BB_ENV_CONFIG_FILE );
    new_len = PAD4(env_length + config_len + sizeof(struct envfs_inode) + PAD4(name_len+1));

    new_env = calloc(new_len + 1, 1);
    memcpy(new_env, *environment, sizeof(struct envfs_super));
    buf            = new_env + sizeof(struct envfs_super);
    inode          = buf;
    inode->magic   = ENVFS_32(ENVFS_INODE_MAGIC);
    inode->namelen = ENVFS_32(name_len + 1);
    inode->size    = ENVFS_32(config_len);
    buf += sizeof(struct envfs_inode);
    memcpy(buf, BB_ENV_CONFIG_FILE, name_len);
    buf += PAD4(name_len + 1);
    memcpy(buf, config, config_len);
    buf += PAD4(config_len);
    /* Copy the rest of the saved environment over... */
    memcpy(buf, (*environment) + sizeof(struct envfs_super), env_length - sizeof(struct envfs_super));

    /* fix up new superblock... */
    super = (struct envfs_super *) new_env;
    super->magic  = ENVFS_32(ENVFS_MAGIC);
    super->size   = ENVFS_32( new_len - sizeof(struct envfs_super));
    super->crc    = ENVFS_32(crc32(0, new_env + sizeof(struct envfs_super), super->size ));
    super->sb_crc = 0;
    super->sb_crc = ENVFS_32(crc32(0, new_env, sizeof(struct envfs_super) - 4));

    envfd = open(new_file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (envfd < 0) {
        fprintf(stderr, "Open %s %d\n", new_file, errno);
        ret = envfd;
        goto out1;
    }

    if (write(envfd, new_env, new_len) < sizeof(struct envfs_super)) {
        perror("write");
        ret = -1;   /* FIXME */
        goto out;
    }

    ret = 0;

out:
    close(envfd);
out1:
    free(new_env);
    return ret;
}


int erase_flash(char* new_file)
{
	mtd_info_t mtd_info;           // the MTD structure
	erase_info_t ei;               // the erase block structure
	int i;


	int fd = open( new_file, O_RDWR);   // open the mtd device for reading and 
		                            // writing. Note you want mtd0 not mtdblock0
		                            // also you probably need to open permissions
		                            // to the dev (sudo chmod 777 /dev/mtd0)

	ioctl(fd, MEMGETINFO, &mtd_info);   // get the device info

	// dump it for a sanity check, should match what's in /proc/mtd
	printf("MTD Type: %x\nMTD total size: %x bytes\nMTD erase size: %x bytes\n",
		mtd_info.type, mtd_info.size, mtd_info.erasesize);

	ei.length = mtd_info.erasesize;   //set the erase block size
	for(ei.start = 0; ei.start < mtd_info.size; ei.start += ei.length)
	{
		ioctl(fd, MEMUNLOCK, &ei);
		// printf("Eraseing Block %#x\n", ei.start); // show the blocks erasing
				                                  // warning, this prints a lot!
		ioctl(fd, MEMERASE, &ei);
	}    
}



void usage(char *prgname)
{
	fprintf(stderr,  "Usage : %s [OPTION] ENV_FILE [NEW_FILE new_vars]\n"
		"Load an barebox environment sector into a directory or\n"
		"save a directory into an barebox environment sector\n"
		"\n"
		"options:\n"
		"  -u        update config given new variables (add vars\n"
		"            at end of cmd line var=val;var=val with no spaces)\n"
		"  -v        print value of a specific variable, given variable name\n"
		"  -p        print all variables in config\n"
		"  -e        erase. assume NEW_FILE is a flash device\n",
		prgname);
}


int main(int argc, char *argv[])
{
	int opt;
	int update = 0, variable = 0, print = 0, erase=0, defenv=0;
	int fd;
	char*         filename      = NULL;
	char*         new_file      = NULL;
	char*         default_file  = NULL;
	struct var*   vars          = NULL;
	struct var*   new_vars      = NULL;
	char*         config        = NULL;
	char*         environment   = NULL;
	char*         varname;
	int           start_new_vars;
	int           env_length;
	int           rc = 0;\
	int           retry_count;

	while((opt = getopt(argc, argv, "ued:pv:")) != -1) {

		switch (opt) {

			case 'u':
				update = 1;
				break;

			case 'e':
				erase = 1;
				break;

			case 'p':
				print = 1;
				break;

			case 'v':
				variable = 1;
				varname = optarg;
				break;

			case 'd':
				defenv = 1;
				default_file = optarg;
				break;
		}
	}

	if ( !update && !print && !variable ) {
		usage(argv[0]);
		exit(1);
	}

	filename  = argv[optind];

	if (update) {

		if (argc < 5) {
			fprintf(stderr, "You need at least four parms\n");
			usage(argv[0]);
			exit(1);
		}

		new_file = argv[optind + 1];
		start_new_vars = optind + 2;

	}

	if (print || variable) {

		if( envfs_find_config( filename, &environment, &env_length, &config) == 0 ) {
			parse_variables(config, &vars);
			if (print)
				print_variables(vars);
			if (variable)
				print_var_value(vars, varname);
			free_variables(&vars);
			free(config);
			free( environment );
		}
	}

	if (update) {

		// Try to open input env file...
		if( envfs_find_config( filename, &environment, &env_length, &config) != 0 ) {

			// Error. See if there's a default env file we should use...
			if( defenv && default_file != NULL ) {

				fprintf(stderr, "Use default env file\n");
				if( envfs_find_config( default_file, &environment, &env_length, &config) != 0 ) {
					exit(-1);
				}
			} else {
				exit(-1);
			}
		}

		parse_variables(config, &vars);
		while(start_new_vars < argc) {
			parse_variables(argv[start_new_vars], &new_vars);
			start_new_vars++;
		}

		parse_and_replace_vars(&vars, &new_vars);

		add_back_vars_to_config(&vars, &config);

		retry_count = 0;   // If writing to flash, retry several times...

		do {

			char*         verify_env;
			char*         verify_config;
			int           verify_env_length;

			if (erase) {
				erase_flash(new_file);
			}

			envfs_save( new_file, &environment, env_length, config);

			if (erase) {

				sync();

				// if we are talking to flash, confirm that we updated it ok.  Read it back in...
				rc = envfs_find_config( new_file, &verify_env, &verify_env_length, &verify_config);

				if(verify_config)
				free(verify_config);
				if(verify_env)
				free(verify_env);

				retry_count++;

				if (rc != 0 && retry_count <= FLASH_WRITE_RETRY_MAX) {
					fprintf(stderr, "Error verifying flash.  Retrying flash write.  Attempt = %d\n", retry_count);
				}
			}

		} while ( erase && rc != 0 && retry_count <= FLASH_WRITE_RETRY_MAX );



		//print_variables(vars);
		free_variables(&vars);
		free(config);
		free( environment );

	}


	exit(0);
}





