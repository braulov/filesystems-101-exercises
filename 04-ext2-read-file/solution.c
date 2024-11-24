#include <solution.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fs_malloc.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_fs.h>
#include <errno.h>
#include <unistd.h>

int process_block(int disk, void* temp_buf, int* bytes_remaining, int blk_sz, int blk_num, int output) {
    if (pread(disk, temp_buf, blk_sz, blk_sz * blk_num) < blk_sz) {
        return -errno;
    }

    int bytes_to_copy = (*bytes_remaining < blk_sz) ? *bytes_remaining : blk_sz;
    if (write(output, temp_buf, bytes_to_copy) < bytes_to_copy) {
        return -errno;
    }

    *bytes_remaining -= bytes_to_copy;
    return 0;
}

int dump_file(int disk, int node_index, int output) {
    struct ext2_super_block sb;
    if (pread(disk, &sb, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET) < 0) {
        return -errno;
    }

    int grp_index = (node_index - 1) / sb.s_inodes_per_group;
    int node_offset = (node_index - 1) % sb.s_inodes_per_group;
    int blk_sz = EXT2_BLOCK_SIZE(&sb);

    struct ext2_group_desc grp_desc;
    if (pread(disk, &grp_desc, sizeof(grp_desc), blk_sz * (sb.s_first_data_block + 1) + sizeof(grp_desc) * grp_index) < 0) {
        return -errno;
    }

    struct ext2_inode inode_data;
    if (pread(disk, &inode_data, sizeof(inode_data), blk_sz * grp_desc.bg_inode_table + sb.s_inode_size * node_offset) < 0) {
        return -errno;
    }

    int bytes_left = inode_data.i_size;
    void* primary_block = fs_xmalloc(blk_sz);
    void* single_indirect_block = NULL;
    void* double_indirect_block = NULL;
    int block_iter = 0;
    int process_res = 0;

    while (block_iter < EXT2_N_BLOCKS && bytes_left > 0 && inode_data.i_block[block_iter] != 0) {
        if (block_iter < EXT2_NDIR_BLOCKS) {
            process_res = process_block(disk, primary_block, &bytes_left, blk_sz, inode_data.i_block[block_iter], output);
            if (process_res < 0) {
                free(primary_block);
                return process_res;
            }
        }

        if (block_iter == EXT2_IND_BLOCK && inode_data.i_block[block_iter] != 0) {
            if (pread(disk, primary_block, blk_sz, blk_sz * inode_data.i_block[block_iter]) < blk_sz) {
                free(primary_block);
                return -errno;
            }
            if (!single_indirect_block) {
                single_indirect_block = fs_xmalloc(blk_sz);
            }

            size_t sub_index = 0;
            while (sub_index < (blk_sz / sizeof(int)) && bytes_left > 0 && ((int*)primary_block)[sub_index] != 0) {
                process_res = process_block(disk, single_indirect_block, &bytes_left, blk_sz, ((int*)primary_block)[sub_index], output);
                if (process_res < 0) {
                    free(primary_block);
                    free(single_indirect_block);
                    return process_res;
                }
                sub_index++;
            }
        }

        if (block_iter == EXT2_DIND_BLOCK) {
            if (pread(disk, primary_block, blk_sz, blk_sz * inode_data.i_block[block_iter]) < blk_sz) {
                free(primary_block);
                free(single_indirect_block);
                return -errno;
            }
            if (!single_indirect_block) {
                single_indirect_block = fs_xmalloc(blk_sz);
            }

            size_t single_idx = 0;
            while (single_idx < (blk_sz / sizeof(int)) && bytes_left > 0 && ((int*)primary_block)[single_idx] != 0) {
                if (pread(disk, single_indirect_block, blk_sz, blk_sz * ((int*)primary_block)[single_idx]) < blk_sz) {
                    free(primary_block);
                    free(single_indirect_block);
                    free(double_indirect_block);
                    return -errno;
                }

                if (!double_indirect_block) {
                    double_indirect_block = fs_xmalloc(blk_sz);
                }

                size_t double_idx = 0;
                while (double_idx < (blk_sz / sizeof(int)) && bytes_left > 0 && ((int*)single_indirect_block)[double_idx] != 0) {
                    process_res = process_block(disk, double_indirect_block, &bytes_left, blk_sz, ((int*)single_indirect_block)[double_idx], output);
                    if (process_res < 0) {
                        free(primary_block);
                        free(single_indirect_block);
                        free(double_indirect_block);
                        return process_res;
                    }
                    double_idx++;
                }
                single_idx++;
            }
        }
        block_iter++;
    }

    free(primary_block);
    if (single_indirect_block) {
        free(single_indirect_block);
    }
    if (double_indirect_block) {
        free(double_indirect_block);
    }

    return 0;
}
