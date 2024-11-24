#include <solution.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fs_malloc.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_fs.h>
#include <errno.h>
#include <unistd.h>

int read_block(int img, void* buffer, int* left_to_copy, int block_size, int block, int out) {
    if (pread(img, buffer, block_size, block_size * block) < block_size) {
        return -errno;
    }

    int size_to_write = (block_size < *left_to_copy) ? block_size : *left_to_copy;
    if (write(out, buffer, size_to_write) < size_to_write) {
        return -errno;
    }

    *left_to_copy -= size_to_write;
    return 0;
}

int dump_file(int img, int inode_nr, int out) {
    struct ext2_super_block super_block;
    if (pread(img, &super_block, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET) < 0) {
        return -errno;
    }

    int group_id = (inode_nr - 1) / super_block.s_inodes_per_group;
    int inode_id = (inode_nr - 1) % super_block.s_inodes_per_group;
    int block_size = EXT2_BLOCK_SIZE(&super_block);

    struct ext2_group_desc group_desc;
    if (pread(img, &group_desc, sizeof(group_desc), block_size * (super_block.s_first_data_block + 1) + sizeof(group_desc) * group_id) < 0) {
        return -errno;
    }

    struct ext2_inode inode;
    if (pread(img, &inode, sizeof(inode), block_size * group_desc.bg_inode_table + super_block.s_inode_size * inode_id) < 0) {
        return -errno;
    }

    int left_to_copy = inode.i_size;
    void* direct_block = fs_xmalloc(block_size);
    void* indirect_block = NULL;
    void* double_indirect_block = NULL;
    int i = 0;
    int read_block_result = 0;

    while (i < EXT2_N_BLOCKS && left_to_copy > 0 && inode.i_block[i] != 0) {
        if (i < EXT2_NDIR_BLOCKS) {
            read_block_result = read_block(img, direct_block, &left_to_copy, block_size, inode.i_block[i], out);
            if (read_block_result < 0) {
                free(direct_block);
                return read_block_result;
            }
        }

        if (i == EXT2_IND_BLOCK && inode.i_block[i] != 0) {
            if (pread(img, direct_block, block_size, block_size * inode.i_block[i]) < block_size) {
                free(direct_block);
                return -errno;
            }
            if (!indirect_block) {
                indirect_block = fs_xmalloc(block_size);
            }

            int k = 0;
            while (k < (block_size / (int)sizeof(int)) && left_to_copy > 0 && ((int*)direct_block)[k] != 0) {
                read_block_result = read_block(img, indirect_block, &left_to_copy, block_size, ((int*)direct_block)[k], out);
                if (read_block_result < 0) {
                    free(direct_block);
                    free(indirect_block);
                    return read_block_result;
                }
                k++;
            }
        }

        if (i == EXT2_DIND_BLOCK) {
            if (pread(img, direct_block, block_size, block_size * inode.i_block[i]) < block_size) {
                free(direct_block);
                free(indirect_block);
                return -errno;
            }
            if (!indirect_block) {
                indirect_block = fs_xmalloc(block_size);
            }

            int k = 0;
            while (k < (block_size / (int)sizeof(int)) && left_to_copy > 0 && ((int*)direct_block)[k] != 0) {
                if (pread(img, indirect_block, block_size, block_size * ((int*)direct_block)[k]) < block_size) {
                    free(direct_block);
                    free(indirect_block);
                    free(double_indirect_block);
                    return -errno;
                }

                if (!double_indirect_block) {
                    double_indirect_block = fs_xmalloc(block_size);
                }

                int n = 0;
                while (n < (block_size / (int)sizeof(int)) && left_to_copy > 0 && ((int*)indirect_block)[n] != 0) {
                    read_block_result = read_block(img, double_indirect_block, &left_to_copy, block_size, ((int*)indirect_block)[n], out);
                    if (read_block_result < 0) {
                        free(direct_block);
                        free(indirect_block);
                        free(double_indirect_block);
                        return read_block_result;
                    }
                    n++;
                }
                k++;
            }
        }
        i++;
    }

    free(direct_block);
    if (indirect_block) {
        free(indirect_block);
    }
    if (double_indirect_block) {
        free(double_indirect_block);
    }

    return 0;
}
