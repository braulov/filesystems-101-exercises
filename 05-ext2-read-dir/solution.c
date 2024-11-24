#include <solution.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fs_malloc.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_fs.h>
#include <errno.h>
#include <unistd.h>

int fetch_block(int img, int* buffer, int* remaining_bytes, int block_size, int block) {
    if (pread(img, buffer, block_size, block_size * block) < block_size) {
        fprintf(stderr, "Error reading block %d\n", block);
        return -errno;
    }

    int process_size = (*remaining_bytes < block_size) ? *remaining_bytes : block_size;
    *remaining_bytes -= process_size;
    int offset = 0;

    while (offset < process_size) {
        struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)((char*)buffer + offset);

        if (entry->inode == 0) {
            break;
        }

        if (entry->rec_len == 0 || offset + entry->rec_len > block_size) {
            fprintf(stderr, "Invalid directory entry length (inode: %d, rec_len: %d)\n", entry->inode, entry->rec_len);
            return -EINVAL;
        }

        char filename[EXT2_NAME_LEN + 1] = {0};
        memcpy(filename, entry->name, entry->name_len);
        filename[entry->name_len] = '\0';

        char entry_type = '?';
        if (entry->file_type == EXT2_FT_DIR) {
            entry_type = 'd';
        } else if (entry->file_type == EXT2_FT_REG_FILE) {
            entry_type = 'f';
        } else {
            fprintf(stderr, "Unknown file type: %d\n", entry->file_type);
            return -EINVAL;
        }

        report_file(entry->inode, entry_type, filename);

        offset += entry->rec_len;
    }

    return 0;
}

int dump_dir(int img, int inode_number) {
    struct ext2_super_block sb;
    int superblock_status = pread(img, &sb, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET);
    if (superblock_status < 0) {
        fprintf(stderr, "Error reading superblock\n");
        return -errno;
    }

    int group_index = (inode_number - 1) / sb.s_inodes_per_group;
    int inode_index = (inode_number - 1) % sb.s_inodes_per_group;
    int block_size = EXT2_BLOCK_SIZE(&sb);

    struct ext2_group_desc group_descriptor;
    int group_desc_status = pread(img, &group_descriptor, sizeof(group_descriptor), block_size * (sb.s_first_data_block + 1) + sizeof(group_descriptor) * group_index);
    if (group_desc_status < 0) {
        fprintf(stderr, "Error reading group descriptor\n");
        return -errno;
    }

    struct ext2_inode inode;
    int inode_status = pread(img, &inode, sizeof(inode), block_size * group_descriptor.bg_inode_table + sb.s_inode_size * inode_index);
    if (inode_status < 0) {
        fprintf(stderr, "Error reading inode\n");
        return -errno;
    }

    int remaining_bytes = inode.i_size;
    int* block_buffer = fs_xmalloc(block_size);
    int* indirect_buffer = NULL;
    int* double_indirect_buffer = NULL;
    int block_index = 0;
    int read_status = 0;

    while (block_index < EXT2_N_BLOCKS && remaining_bytes > 0 && inode.i_block[block_index] != 0) {
        if (block_index < EXT2_NDIR_BLOCKS) {
            read_status = fetch_block(img, block_buffer, &remaining_bytes, block_size, inode.i_block[block_index]);
            if (read_status < 0) {
                free(block_buffer);
                return read_status;
            }
        }

        if (block_index == EXT2_IND_BLOCK) {
            if (pread(img, block_buffer, block_size, block_size * inode.i_block[block_index]) < block_size) {
                free(block_buffer);
                return -errno;
            }
            if (!indirect_buffer) {
                indirect_buffer = fs_xmalloc(block_size);
            }

            size_t sub_block = 0;
            while (sub_block < (block_size / sizeof(int)) && block_buffer[sub_block] != 0 && remaining_bytes > 0) {
                read_status = fetch_block(img, indirect_buffer, &remaining_bytes, block_size, block_buffer[sub_block]);
                if (read_status < 0) {
                    free(block_buffer);
                    free(indirect_buffer);
                    return read_status;
                }
                sub_block++;
            }
        }

        if (block_index == EXT2_DIND_BLOCK) {
            if (pread(img, block_buffer, block_size, block_size * inode.i_block[block_index]) < block_size) {
                free(block_buffer);
                free(indirect_buffer);
                return -errno;
            }
            if (!indirect_buffer) {
                indirect_buffer = fs_xmalloc(block_size);
            }

            size_t sub_block = 0;
            while (sub_block < (block_size / sizeof(int)) && remaining_bytes > 0 && block_buffer[sub_block] != 0) {
                if (pread(img, indirect_buffer, block_size, block_size * block_buffer[sub_block]) < block_size) {
                    free(block_buffer);
                    free(indirect_buffer);
                    free(double_indirect_buffer);
                    return -errno;
                }

                if (!double_indirect_buffer) {
                    double_indirect_buffer = fs_xmalloc(block_size);
                }

                size_t sub_sub_block = 0;
                while (sub_sub_block < (block_size / sizeof(int)) && remaining_bytes > 0 && indirect_buffer[sub_sub_block] != 0) {
                    read_status = fetch_block(img, double_indirect_buffer, &remaining_bytes, block_size, indirect_buffer[sub_sub_block]);
                    if (read_status < 0) {
                        free(block_buffer);
                        free(indirect_buffer);
                        free(double_indirect_buffer);
                        return read_status;
                    }
                    sub_sub_block++;
                }
                sub_block++;
            }
        }
        block_index++;
    }

    free(block_buffer);
    if (indirect_buffer) {
        free(indirect_buffer);
    }
    if (double_indirect_buffer) {
        free(double_indirect_buffer);
    }

    return 0;
}
