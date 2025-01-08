#ifndef FS_H
#define FS_H

#include <stddef.h> // For size_t
#include <sys/types.h> // For off_t

#define MAX_FILES 64
#define MAX_FILENAME 15
#define MAX_FILDES 32
#define BLOCK_SIZE 4096
#define BLOCKS_TOTAL 8192
#define BLOCKS_DATA 4096

/* File System Management Functions */
int make_fs(char *disk_name);
int mount_fs(char *disk_name);
int umount_fs(char *disk_name);

/* File Operations */
int fs_create(char *name);
int fs_delete(char *name);
int fs_open(char *name);
int fs_close(int fildes);
int fs_read(int fildes, void *buf, size_t nbyte);
int fs_write(int fildes, void *buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_listfiles(char ***files);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);

#endif // FS_H
