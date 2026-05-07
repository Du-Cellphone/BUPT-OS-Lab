#ifndef MEMFS_H
#define MEMFS_H

#define MEMFS_NAME_LEN        16
#define MEMFS_CLUSTER_SIZE    512
#define MEMFS_CLUSTER_COUNT   2048

/* FAT entry values */
#define MEMFS_FAT_FREE        0x0000
#define MEMFS_FAT_EOC         0xfffe

void memfs_init(struct MEMMAN *memman);
int memfs_ready(void);

int memfs_mkdir(char *path);
int memfs_create(char *path);
int memfs_write_text(char *path, char *text);
int memfs_read_text(char *path, char *out, int outmax);
int memfs_copy(char *src_path, char *dst_path);
int memfs_remove(char *path);
int memfs_rmdir(char *path);
int memfs_ls(char *path, char *out, int outmax);

#endif
