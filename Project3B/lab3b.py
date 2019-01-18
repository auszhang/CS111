# NAME: Austin Zhang
# EMAIL: aus.zhang@gmail.com
# ID: 604736503

import sys

# create a set of free blocks and a set of free inodes, respectively
free_blocks = set()
free_inodes = set()

# create some dictionaries...
blocknum_block = {} 
inodenum_lc = {} 
inodenum_refc = {}
inodenum_parent = {}
parent_inodenum = {}
inodenum_dir = {}

# declarations of some variables
invalid_file = False
linkcount = 0
links = 0
num_blocks  = 0
num_inodes = 0
offset = 0

indirect_level = ["INDIRECT", "DOUBLE INDIRECT", "TRIPLE INDIRECT"]
single_offset = 12
double_offset = 268
triple_offset = 65804
reserved = set([0, 1, 2, 3, 4, 5, 6, 7, 64])

csv_filename = sys.argv[1]

# open file
try:
	input_file = open(csv_filename, "r")
except:
	sys.stderr.write('Error: could not find file\n')
	exit(1)

lines = input_file.readlines()

# parse file
for i in lines:
	entry = i.split(",") # split into comma separated items
	type = entry[0]

	if type == 'IFREE': # free inodes
		free_inodes.add(int(entry[1])) 
	elif type == 'BFREE': # free blocks
		free_blocks.add(int(entry[1]))
	elif type == 'SUPERBLOCK': 
		num_blocks  = int(entry[1])
		num_inodes = int(entry[2]) 
	elif type == 'INODE': # inode dict
		inode_num = int(entry[1])
		inodenum_lc[inode_num] = int(entry[6])
		for i in range(12, 27): # check block addresses
			block_num = int(entry[i])
			if block_num == 0: # unused
				continue
			if i == 24: # SINGLE INDIRECT
				level_string = " " + indirect_level[0]
				level = 1
				offset = single_offset
			elif i == 25: # DOUBLE INDIRECT 
				level_string = " " + indirect_level[1]
				level = 2
				offset = double_offset
			elif i == 26: # TRIPLE INDIRECT
				level_string = " " + indirect_level[2]
				level = 3
				offset = triple_offset
			else: # default
				level_string = ""
				level = 0
				offset = 0

			arr = [inode_num, offset, level]
			if block_num < 0 or block_num > num_blocks : # valid?
				print('INVALID' + level_string + ' BLOCK ' + str(block_num) + ' IN INODE ' + str(inode_num) + ' AT OFFSET ' + str(offset))
				invalid_file = True
			elif block_num in reserved and block_num != 0: # reserved?
				print('RESERVED' + level_string + ' BLOCK ' + str(block_num) + ' IN INODE ' + str(inode_num) + ' AT OFFSET ' + str(offset))
				invalid_file = True
			elif block_num not in blocknum_block:
				blocknum_block[block_num] = [arr]
			else: 
				blocknum_block[block_num].append(arr)

	elif type == 'DIRENT': 
		dir_name = entry[6]
		parent = int(entry[1])
		inode_num = int(entry[3])
		inodenum_dir[inode_num] = dir_name

		if inode_num not in inodenum_refc:
			inodenum_refc[inode_num] = 1
		else:
			inodenum_refc[inode_num] += 1
		
		if inode_num < 1 or inode_num > num_inodes:
			print('DIRECTORY INODE ' + str(parent) + ' NAME ' + dir_name[0:len(dir_name)- 2] + "' INVALID INODE " + str(inode_num))
			invalid_file = True
			continue
		
		if dir_name[0:3] == "'.'" and parent != inode_num:
			print('DIRECTORY INODE ' + str(parent) + " NAME '.' LINK TO INODE " + str(inode_num) + ' SHOULD BE ' + str(parent))
			invalid_file = True
		elif dir_name[0:3] == "'.'":
			continue
		elif dir_name[0:4] == "'..'":
			parent_inodenum[parent] = inode_num
		else:
			inodenum_parent[inode_num] = parent

	elif type == 'INDIRECT':
		block_num = int(entry[5])
		inode_num = int(entry[1])

		level = int(entry[2])
		if level == 1: # SINGLE INDIRECT
			level_string = indirect_level[0]
			offset = single_offset
		elif level == 2: # DOUBLE INDIRECT
			level_string = indirect_level[1]
			offset = double_offset
		elif level == 3: # TRIPLE INDIRECT
			level_string = indirect_level[2]
			offset = triple_offset

		arr = [inode_num, offset, level]
		if block_num < 0 or block_num > num_blocks : #valid?
			print('INVALID ' + level_string + ' BLOCK ' + str(block_num) + ' IN INODE ' + str(inode_num) + ' AT OFFSET ' + str(offset))
			invalid_file = True
		elif block_num in reserved: # reserved?
			print('RESERVED ' + level_string + ' BLOCK ' + str(block_num) + ' IN INODE ' + str(inode_num)+ ' AT OFFSET ' + str(offset))
			invalid_file = True
		elif block_num not in blocknum_block: 
			blocknum_block[block_num] = [arr]
		else: 
			blocknum_block[block_num].append(arr)

# check if every legal data block appears on the free block list, or is allocated to exactly one file
for j in range(1, num_blocks  + 1):
	if j not in free_blocks and j not in blocknum_block and j not in reserved:
		print('UNREFERENCED BLOCK ' + str(j))
		invalid_file = True
	elif j in free_blocks and j in blocknum_block:
		print('ALLOCATED BLOCK ' + str(j) + ' ON FREELIST')
		invalid_file = True

# compare list of allocated/unallocated inodes with the free inode bitmaps and generate message for each inconsistency
for k in range(1, num_inodes + 1):
	if k in inodenum_lc and k in free_inodes:
		print('ALLOCATED INODE ' + str(k) + ' ON FREELIST')
		invalid_file = True
	elif k not in free_inodes and k not in inodenum_lc and k not in inodenum_parent and k > 10:
		print('UNALLOCATED INODE ' + str(k) + ' NOT ON FREELIST')
		invalid_file = True
	
# generate message for multiple references of a legal block
for b in blocknum_block:
	if len(blocknum_block[b]) > 1: # if multiple references
		invalid_file = True
		for ref in blocknum_block[b]:
			level = int(ref[2])
			if level == 1:
				level_string = " " + indirect_level[0]
			elif level == 2:
				level_string = " " + indirect_level[1]
			elif level == 3:
				level_string = " " + indirect_level[2]
			elif level == 0: 
				level_string = ""

			# print duplicates
			print('DUPLICATE' + level_string + ' BLOCK ' + str(b) + ' IN INODE ' + str(ref[0]) + ' AT OFFSET ' + str(ref[1]))


# generate message for detected inconsistency in directory links
for p in parent_inodenum:
	if parent_inodenum[p] == 2 and p == 2:
		continue
	elif p == 2 and parent_inodenum[p] != 2:
		print("DIRECTORY INODE 2 NAME '..' LINK TO INODE " + str(parent_inodenum[p]) + " SHOULD BE 2")
		invalid_file = True
	elif parent_inodenum[p] != inodenum_parent[p]: 
		print("DIRECTORY INODE " + str(p) + " NAME '..' LINK TO INODE " + str(p) + " SHOULD BE " + str(inodenum_parent[p]))
		invalid_file = True
	elif p not in inodenum_parent:
		print("DIRECTORY INODE " + str(parent_inodenum[p]) + " NAME '..' LINK TO INODE " + str(p) + " SHOULD BE " + str(parent_inodenum[p]))
		invalid_file = True


# generate message for inode whose reference count does not match the number of discovered links
for c in inodenum_lc:
	linkcount = inodenum_lc[c]
	if c not in inodenum_refc:
		links = 0
	else:
		links = inodenum_refc[c]
	if links != linkcount:
		print('INODE ' + str(c) + ' HAS ' + str(links) + ' LINKS BUT LINKCOUNT IS ' + str(linkcount))
		invalid_file = True


# generate message for invalid or unallocated inodes 
for d in inodenum_dir:
	if d in inodenum_parent and d in free_inodes:
		dir_name = inodenum_dir[d]
		parent = inodenum_parent[d]
		print('DIRECTORY INODE ' + str(parent) + ' NAME ' + dir_name[0:len(dir_name)- 2] + "' UNALLOCATED INODE " + str(d))
		invalid_file = True

if invalid_file:
	exit(2)
else:
	exit(0)