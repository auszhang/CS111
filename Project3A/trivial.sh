
#!/bin/bash
#
# create a trivial file system to demonstrate/test analyzer
#
# output:
#	trivial.img	EXT2 file system image
#	trivial.csv	summary of file system data structures
#
#	You can run this script any time you want, but the 
#	resulting file system is always exactly the same.
#
#

# file system size
BLOCKS=64
INODES=20

# I-node/Indirect block sizes
BSIZE=1024
PWIDTH=4
IPTRS=12

# file names
FILE="trivial.img"
OUTPUT="trivial.csv"
MOUNTPOINT=/tmp/test.$$
PGM=../SOLUTION/lab3a

echo "... creating empty image:" $FILE
dd if=/dev/zero of=$FILE bs=$BSIZE count=$BLOCKS			2>/dev/null

echo "... creating a small EXT2 file system in" $FILE
/sbin/mkfs -b $BSIZE -t ext2 -m 0 -O ^resize_inode -N $INODES $FILE

echo
echo "... mounting new file system as" $MOUNTPOINT
mkdir $MOUNTPOINT
sudo mount -o loop $FILE $MOUNTPOINT
sudo chown -R $USER $MOUNTPOINT

echo ... creating a subdirectory
mkdir $MOUNTPOINT/SUBDIRECTORY_1

echo ... creating empty files
touch $MOUNTPOINT/empty_file
touch $MOUNTPOINT/SUBDIRECTORY_1/empty_file

echo ... creating hard link
ln $MOUNTPOINT/empty_file $MOUNTPOINT/link_to_empty_file

echo ... creating symbolic link
ln -s $MOUNTPOINT/empty_file $MOUNTPOINT/symlink_to_empty_file

echo ... creating small file
dd if=/dev/urandom of=$MOUNTPOINT/small_file bs=$BSIZE count=1		2>/dev/null

echo ... creating large file
let large=IPTRS+2
dd if=/dev/urandom of=$MOUNTPOINT/large_file bs=$BSIZE count=$large	2>/dev/null

echo ... creating sparse file with single, double, and triple indirect blocks
let ppb=BSIZE/PWIDTH
let huge=large+ppb
let epic=ppb*ppb+huge
dd if=/dev/urandom of=$MOUNTPOINT/sparse_file bs=$BSIZE count=1 seek=$large		2>/dev/null
dd if=/dev/urandom of=$MOUNTPOINT/sparse_file bs=$BSIZE count=1 seek=$huge conv=notrunc	2>/dev/null
dd if=/dev/urandom of=$MOUNTPOINT/sparse_file bs=$BSIZE count=1 seek=$epic conv=notrunc	2>/dev/null

echo "... unmounting and cleaning up"
sudo umount $MOUNTPOINT
rm -rf $MOUNTPOINT

echo
echo "... generating golden output:" $OUTPUT
$PGM -v $FILE > $OUTPUT
