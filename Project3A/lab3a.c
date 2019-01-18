/*
 * NAME: Austin Zhang
 * EMAIL: aus.zhang@gmail.com
 * ID: 604736503
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include "ext2_fs.h"

#define OFFSET 1024

int disk_fd;
unsigned int block_size;
struct ext2_super_block superblock;

void print_error(char* message) {
	fprintf(stderr, message);
	exit(1);
}


// store the time in the format mm/dd/yy hh:mm:ss, GMT
// 'seconds': number of seconds since 01/01/1970
void get_time(time_t seconds, char* buf) {
	time_t epoch = seconds;
	struct tm ts = *gmtime(&epoch);
	strftime(buf, 80, "%m/%d/%y %H:%M:%S", &ts);
}

// returns offset for a block
unsigned long block_offset(unsigned int block) {
	return OFFSET + (block - 1) * block_size;
}

// print the number of the free block
void free_blocks(int group, unsigned int block) {
	// 1 = used, 0 = free
	unsigned long offset = block_offset(block);
	char* bytes = (char*) malloc(block_size);
	unsigned int block_num =  group * superblock.s_blocks_per_group + superblock.s_first_data_block;
	pread(disk_fd, bytes, block_size, offset);

	unsigned int i, j;
	for (i = 0; i < block_size; i++) {
		char block = bytes[i];
		for (j = 0; j < sizeof(long); j++) {
			if (!(1 & block)) {
				fprintf(stdout, "BFREE,%d\n", block_num);
			}
			block = block >> 1;
			block_num++;
		}
	}
	free(bytes);
}

// print superblock summary
void superblock_output() {
	pread(disk_fd, &superblock, sizeof(superblock), OFFSET);

	fprintf(stdout, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", 
		superblock.s_blocks_count, // total number of blocks (decimal)
		superblock.s_inodes_count, // total number of inodes (decimal)
		block_size,	// block size (decimal)
		superblock.s_inode_size, // i-node size (decimal)
		superblock.s_blocks_per_group, // blocks per group (decimal)
		superblock.s_inodes_per_group,// inodes per group (decimal)
		superblock.s_first_ino // first non-reserved inode (decimal)
	);
}



// produce directory entry summary
void dir_output(unsigned int parent_inode, unsigned int block_num) {
	unsigned int bytes = 0;
	unsigned long offset = block_offset(block_num);
	struct ext2_dir_entry dir_entry;

	while(block_size > bytes) {
		memset(dir_entry.name, 0, 256);
		pread(disk_fd, &dir_entry, sizeof(dir_entry), offset + bytes);
		if (dir_entry.inode != 0) { 
			memset(&dir_entry.name[dir_entry.name_len], 0, 256 - dir_entry.name_len);
			fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,'%s'\n",
				parent_inode, // parent inode number (decimal)
				bytes, // logical byte offset (decimal)
				dir_entry.inode, // inode number of the referenced file (decimal)
				dir_entry.rec_len, // entry length (decimal)
				dir_entry.name_len, // name length (decimal)
				dir_entry.name // name (string)
			);
		}
		bytes += dir_entry.rec_len;
	}
}

// print summary for an inode
void read_file(unsigned int inode_table_id, unsigned int index, unsigned int inode_num) {
	struct ext2_inode inode;

	unsigned long offset = index * sizeof(inode) + block_offset(inode_table_id);
	if (pread(disk_fd, &inode, sizeof(inode), offset) < 0) {
		print_error("Error: pread failed.\n");
	}

	if (inode.i_mode == 0 || inode.i_links_count == 0) {
		return;
	}

	// determine file type
	char filetype = '?';
	uint16_t file_val = (inode.i_mode >> 12) << 12;
	if (file_val == 0x8000) {
		filetype = 'f';
	} else if (file_val == 0x4000) {
		filetype = 'd';
	} else if (file_val == 0xa000) {
		filetype = 's';
	}


	unsigned int num_blocks = 2 * (inode.i_blocks / (2 << superblock.s_log_block_size));

	fprintf(stdout, "INODE,%d,%c,%o,%d,%d,%d,",
		inode_num, // inode number (decimal)
		filetype, // filetype
		inode.i_mode & 0xFFF, // mode (low order 12-bits)
		inode.i_uid, // owner (decimal)
		inode.i_gid, // group (decimal)
		inode.i_links_count // link count (decimal)
	);

	char ctime[20], mtime[20], atime[20];
    	get_time(inode.i_ctime, ctime); // time of last inode change (mm/dd/yy hh:mm:ss, GMT)
    	get_time(inode.i_mtime, mtime); // modification time (mm/dd/yy hh:mm:ss, GMT)
    	get_time(inode.i_atime, atime); // access time (mm/dd/yy hh:mm:ss, GMT)
    	fprintf(stdout, "%s,%s,%s,", ctime, mtime, atime);
		
	fprintf(stdout, "%d,%d", 
	    inode.i_size, // file size (decimal)
		num_blocks // number of blocks
	);

	// print block addresses
	unsigned int i;
	for (i = 0; i < 15; i++) { 
		fprintf(stdout, ",%d", inode.i_block[i]);
	}
	fprintf(stdout, "\n");

	// create a directory entry if the filetype is a directory
	if (filetype == 'd') {
		for (i = 0; i < 12; i++) {
			if (inode.i_block[i] != 0) {
				dir_output(inode_num, inode.i_block[i]);
			}
		}
	}

	// single indirect entry
	if (inode.i_block[12] != 0) {
		uint32_t num_p = block_size / 32;
		uint32_t *block_p = malloc(block_size);
		unsigned long indirect_offset = block_offset(inode.i_block[12]);
		pread(disk_fd, block_p, block_size, indirect_offset);

		unsigned int j;
		for (j = 0; j < num_p; j++) {
			if (block_p[j] != 0) {
				if (filetype == 'd') {
					dir_output(inode_num, block_p[j]);
				}
				fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
					inode_num, // inode number of the owning file (decimal)
					1, // level of indirection (single)
					12 + j, // logical block offset (decimal)
					inode.i_block[12], // block number of indirect block (decimal)
					block_p[j] // block number of reference block (decimal)
				);
			}
		}
		free(block_p);
	}

	// double
	if (inode.i_block[13] != 0) {
		uint32_t num_p = block_size / 32;
		uint32_t *db_block_p = malloc(block_size);
		unsigned long indirect_offset_db = block_offset(inode.i_block[13]);
		pread(disk_fd, db_block_p, block_size, indirect_offset_db);

		unsigned int j;
		for (j = 0; j < num_p; j++) {
			if (db_block_p[j] != 0) {
				fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
					inode_num, // inode number of the owning file (decimal)
					2, // level of indirection (sdouble)
					256 + 12 + j, // logical block offset (decimal)
					inode.i_block[13], // block number of indirect block being scanned (decimal)
					db_block_p[j] // block number of reference block (decimal)
				);

				// find directories
				uint32_t *block_p = malloc(block_size);
				unsigned long indirect_offset = block_offset(db_block_p[j]);
				pread(disk_fd, block_p, block_size, indirect_offset);

				unsigned int k;
				for (k = 0; k < num_p; k++) {
					if (block_p[k] != 0) {
						if (filetype == 'd') {
							dir_output(inode_num, block_p[k]);
						}
						fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
							inode_num, // inode number of the owning file (decimal)
							1, // level of indirection (single)
							256 + 12 + k, // logical block offset (decimal)
							db_block_p[j], // block number of indirect block (decimal)
							block_p[k] // block number of reference block (decimal)
						);
					}
				}
				free(block_p);
			}
		}
		free(db_block_p);
	}

	// triple
	if (inode.i_block[14] != 0) {
		uint32_t num_p = block_size / 32;
		uint32_t *tr_block_p = malloc(block_size);
		unsigned long indirect_offset_tr = block_offset(inode.i_block[14]);
		pread(disk_fd, tr_block_p, block_size, indirect_offset_tr);

		unsigned int j;
		for (j = 0; j < num_p; j++) {
			if (tr_block_p[j] != 0) {
				fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
					inode_num, // inode number of the owning file (decimal)
					3, // level of indirection (triple)
					65536 + 256 + 12 + j, // logical block offset (decimal)
					inode.i_block[14], // block number of indirect block (decimal)
					tr_block_p[j] // block number of reference block (decimal)
				);

				uint32_t *db_block_p = malloc(block_size);
				unsigned long indirect_offset_db = block_offset(tr_block_p[j]);
				pread(disk_fd, db_block_p, block_size, indirect_offset_db);

				unsigned int k;
				for (k = 0; k < num_p; k++) {
					if (db_block_p[k] != 0) {
						fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
							inode_num, // inode number of the owning file (decimal)
							2, // level of indirection (double)
							65536 + 256 + 12 + k, // logical block offset (decimal)
				 			tr_block_p[j], // block number of indirect block being scanned (decimal)
							db_block_p[k] // block number of reference block	(decimal)
						);	
						uint32_t *block_p = malloc(block_size);
						unsigned long indirect_offset = block_offset(db_block_p[k]);
						pread(disk_fd, block_p, block_size, indirect_offset);

						unsigned int l;
						for (l = 0; l < num_p; l++) {
							if (block_p[l] != 0) {
								if (filetype == 'd') {
									dir_output(inode_num, block_p[l]);
								}
								fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
									inode_num, // inode number (decimal)
									1, // level of indirection (single)
									65536 + 256 + 12 + l, // logical block offset (decimal)
				 					db_block_p[k], // block number of indirect block being scanned (decimal)
									block_p[l] // block number of reference block (decimal)
								);
							}
						}
						free(block_p);
					}
				}
				free(db_block_p);
			}
		}
		free(tr_block_p);
	}
}

// for each inode, print if free; else, print inode summary 
void read_inode_bitmap(int group, int block, int inode_table_id) {
	int i, j;
	int byte_size = 8;
	int inode_bytes = superblock.s_inodes_per_group / byte_size;
	char* bytes = (char*) malloc(inode_bytes);

	unsigned long offset = block_offset(block);
	unsigned int current_inode = group * superblock.s_inodes_per_group + 1;
	unsigned int start = current_inode;
	
	if (pread(disk_fd, bytes, inode_bytes, offset) < 0 ) {
		print_error("Error: failed to (p)read file descriptor.\n");
	}

	// for each inode...
	for (i = 0; i < inode_bytes; i++) {
		char block = bytes[i];
		for (j = 0; j < byte_size; j++) {
			if (!block) { // if the inode is free
				fprintf(stdout, "IFREE,%d\n", current_inode);
			} else { // not free
				read_file(inode_table_id, current_inode - start, current_inode);
			}
			current_inode++;
			block = block >> 1;
		}
	}
	free(bytes);
}

// read bitmaps for blocks and inodes for all groups
void read_group(int group_num, int total_groups) {
	struct ext2_group_desc group_desc;
	uint32_t d_block = 1;
	if (block_size == 1024) {
		d_block = 2;
	}

	unsigned long offset = 32 * group_num + block_size * d_block;
	pread(disk_fd, &group_desc, sizeof(group_desc), offset);

	unsigned int num_inodes_in_group = superblock.s_inodes_per_group;
	if (group_num < total_groups) {
		num_inodes_in_group = superblock.s_inodes_count - (superblock.s_inodes_per_group * (total_groups - 1));
	}
	unsigned int num_blocks_in_group = superblock.s_blocks_per_group;
	if (group_num < total_groups) {
		num_blocks_in_group = superblock.s_blocks_count - (superblock.s_blocks_per_group * (total_groups - 1));
	}

	fprintf(stdout, "GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
		group_num, //group number (decimal, starting from zero)
		num_blocks_in_group, //total number of blocks in this group (decimal)
		num_inodes_in_group, //total number of inodes (decimal)
		group_desc.bg_free_blocks_count, //number of free blocks (decimal)
		group_desc.bg_free_inodes_count, //number of free inodes (decimal)
		group_desc.bg_block_bitmap, //block number of free block bitmap for this group (decimal)
		group_desc.bg_inode_bitmap, //block number of free inode bitmap for this group (decimal)
		group_desc.bg_inode_table //block number of first block of i-nodes in this group (decimal)
	);

	unsigned int block_bitmap = group_desc.bg_block_bitmap;
	unsigned int inode_bitmap = group_desc.bg_inode_bitmap;
	unsigned int inode_table = group_desc.bg_inode_table;
	
	free_blocks(group_num, block_bitmap);
	read_inode_bitmap(group_num, inode_bitmap, inode_table);
}

int main (int argc, char* argv[]) {
	int i; 

	if (argc != 2) {
		print_error("Error: invalid arguments.\n");
	}

	// open file descriptor
	if ((disk_fd = open(argv[1], O_RDONLY)) < 0) {
		print_error("Error: Could not open file.\n");
	}

	block_size = EXT2_MIN_BLOCK_SIZE << superblock.s_log_block_size;
	superblock_output();

	int num_groups = superblock.s_blocks_count / superblock.s_blocks_per_group;
	if ((double) superblock.s_blocks_count / superblock.s_blocks_per_group > (double) num_groups ) {
		num_groups += 1;
	}

	for (i = 0; i < num_groups; i++) {
		read_group(i, num_groups);
	}

	return 0;
}