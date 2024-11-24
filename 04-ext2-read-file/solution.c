#include "solution.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fs_malloc.h>
#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>

struct ext2_fs {
    int file_descriptor;
    int blk_size;
    struct ext2_super_block super_block;
};

struct ext2_blkiter {
    struct ext2_fs *filesystem;
    int inode_table_offset;

    struct ext2_inode current_inode;
    int iterator_position;

    int *indirect_blk_buffer;
    int *double_indirect_blk_buffer;
    int active_indirect_block;
};

int ext2_fs_init(struct ext2_fs **fs, int fd) {
    struct ext2_fs *temp_fs = fs_xmalloc(sizeof(struct ext2_fs));
    temp_fs->file_descriptor = fd;

    ssize_t read_result = pread(temp_fs->file_descriptor, &temp_fs->super_block, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET);
    if (read_result == -1) {
        fs_xfree(temp_fs);
        return -errno;
    }

    temp_fs->blk_size = EXT2_BLOCK_SIZE(&temp_fs->super_block);
    *fs = temp_fs;

    return 0;
}

void ext2_fs_free(struct ext2_fs *fs) {
    fs_xfree(fs);
}

int ext2_blkiter_init(struct ext2_blkiter **iter, struct ext2_fs *fs, int inode_number) {
    size_t descs_per_block = fs->blk_size / sizeof(struct ext2_group_desc);
    size_t inode_group_idx = (inode_number - 1) / fs->super_block.s_inodes_per_group;
    size_t inode_idx_in_group = (inode_number - 1) % fs->super_block.s_inodes_per_group;
    size_t group_desc_offset = fs->blk_size * (fs->super_block.s_first_data_block + 1 + (inode_group_idx / descs_per_block));
    size_t group_desc_within_block = (inode_group_idx % descs_per_block) * sizeof(struct ext2_group_desc);

    struct ext2_group_desc group_desc;
    ssize_t read_group = pread(fs->file_descriptor, &group_desc, sizeof(group_desc), group_desc_offset + group_desc_within_block);
    if (read_group == -1) {
        return -errno;
    }

    struct ext2_blkiter *temp_iter = fs_xmalloc(sizeof(struct ext2_blkiter));
    temp_iter->inode_table_offset = group_desc.bg_inode_table;

    ssize_t read_inode_data = pread(fs->file_descriptor, &temp_iter->current_inode, sizeof(struct ext2_inode),
                                    fs->blk_size * temp_iter->inode_table_offset + inode_idx_in_group * fs->super_block.s_inode_size);
    if (read_inode_data == -1) {
        fs_xfree(temp_iter);
        return -errno;
    }

    temp_iter->iterator_position = 0;
    temp_iter->filesystem = fs;

    temp_iter->indirect_blk_buffer = NULL;
    temp_iter->double_indirect_blk_buffer = NULL;
    temp_iter->active_indirect_block = -1;

    *iter = temp_iter;

    return 0;
}

int ext2_blkiter_next(struct ext2_blkiter *iter, int *block_number) {
    int block_pointers_per_block = iter->filesystem->blk_size / sizeof(int);

    int direct_blocks_end = EXT2_NDIR_BLOCKS;
    int single_indirect_start = direct_blocks_end;
    int single_indirect_end = single_indirect_start + block_pointers_per_block;
    int double_indirect_start = single_indirect_end;
    int double_indirect_end = double_indirect_start + block_pointers_per_block * block_pointers_per_block;

    if (iter->iterator_position < direct_blocks_end) {
        int blk_ptr = iter->current_inode.i_block[iter->iterator_position];
        if (blk_ptr == 0) {
            return 0;
        }

        *block_number = blk_ptr;
        iter->iterator_position++;
        return 1;
    }

    if (iter->iterator_position < single_indirect_end) {
        if (!iter->indirect_blk_buffer) {
            iter->indirect_blk_buffer = fs_xmalloc(iter->filesystem->blk_size);
            ssize_t result = pread(iter->filesystem->file_descriptor, iter->indirect_blk_buffer, iter->filesystem->blk_size,
                                   iter->current_inode.i_block[EXT2_IND_BLOCK] * iter->filesystem->blk_size);
            if (result == -1) {
                return -errno;
            }
        }

        int indirect_idx = iter->iterator_position - single_indirect_start;
        if (iter->indirect_blk_buffer[indirect_idx] == 0) {
            return 0;
        }

        *block_number = iter->indirect_blk_buffer[indirect_idx];
        iter->iterator_position++;
        return 1;
    }

    if (iter->iterator_position < double_indirect_end) {
        if (!iter->double_indirect_blk_buffer) {
            iter->double_indirect_blk_buffer = fs_xmalloc(iter->filesystem->blk_size);
            ssize_t result = pread(iter->filesystem->file_descriptor, iter->double_indirect_blk_buffer,
                                   iter->filesystem->blk_size,
                                   iter->current_inode.i_block[EXT2_DIND_BLOCK] * iter->filesystem->blk_size);
            if (result == -1) {
                return -errno;
            }
        }

        int outer_idx = (iter->iterator_position - double_indirect_start) / block_pointers_per_block;
        int inner_idx = (iter->iterator_position - double_indirect_start) % block_pointers_per_block;

        if (iter->active_indirect_block != iter->double_indirect_blk_buffer[outer_idx]) {
            iter->active_indirect_block = iter->double_indirect_blk_buffer[outer_idx];
            if (pread(iter->filesystem->file_descriptor, iter->indirect_blk_buffer, iter->filesystem->blk_size,
                      iter->active_indirect_block * iter->filesystem->blk_size) == -1) {
                return -errno;
            }
        }

        if (iter->indirect_blk_buffer[inner_idx] == 0) {
            return 0;
        }

        *block_number = iter->indirect_blk_buffer[inner_idx];
        iter->iterator_position++;
        return 1;
    }

    return 0;
}

void ext2_blkiter_free(struct ext2_blkiter *iter) {
    if (iter) {
        fs_xfree(iter->indirect_blk_buffer);
        fs_xfree(iter->double_indirect_blk_buffer);
        fs_xfree(iter);
    }
}

/**
 * Function to copy the content of an inode @inode_nr to a file descriptor @out.
 * @param img File descriptor of the ext2 image.
 * @param inode_nr The inode number to read.
 * @param out File descriptor where the data will be written.
 * @return 0 if successful, or -errno if an error occurs.
 */
int dump_file(int img, int inode_nr, int out) {
    struct ext2_fs *fs = NULL;
    struct ext2_blkiter *blk_iter = NULL;
    int ret = 0;
    int block_number;


    if ((ret = ext2_fs_init(&fs, img)) != 0) {
        return ret;
    }


    if ((ret = ext2_blkiter_init(&blk_iter, fs, inode_nr)) != 0) {
        ext2_fs_free(fs);
        return ret;
    }

    char *block_buffer = fs_xmalloc(fs->blk_size);

    while ((ret = ext2_blkiter_next(blk_iter, &block_number)) > 0) {
        ssize_t bytes_read = pread(img, block_buffer, fs->blk_size, block_number * fs->blk_size);
        if (bytes_read == -1) {
            ret = -errno;
            break;
        }


        ssize_t bytes_written = write(out, block_buffer, bytes_read);
        if (bytes_written == -1) {
            ret = -errno;
            break;
        }

        if (bytes_written < bytes_read) {
            ret = -EIO;
            break;
        }
    }

    if (ret == 0) {
        ret = 0;
    } else if (ret > 0) {
        ret = 0;
    }


    fs_xfree(block_buffer);
    ext2_blkiter_free(blk_iter);
    ext2_fs_free(fs);

    return ret;
}
