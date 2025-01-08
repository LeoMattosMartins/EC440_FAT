#include <stddef.h>
#include <sys/types.h>
#include "disk.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_FILES 64
#define MAX_FILENAME 15
#define MAX_FILDES 32

#define BLOCK_SIZE 4096
#define BLOCKS_TOTAL 8192
#define BLOCKS_DATA 4096

/* global variables */
static struct superBlock
{
        int  fatIdx, fatLen;
        int  dirIdx, dirLen;
        int dataIdx;
} fs;

static struct dirEntry
{
        char name[MAX_FILENAME + 1];
        int used, size, head, refCount, filler[1019];
} *dir;

static struct fileDescriptor
{
        int used, file, offset;
} fds[MAX_FILDES];

static int* fat;
static int numFiles = 0;

/* helper functions */
void fsInit()
{
        fat = calloc(BLOCKS_TOTAL, sizeof(int));
        dir = calloc(MAX_FILES, sizeof(struct dirEntry));
        memset(fds, 0, sizeof(fds));
}

void fsExit()
{
        free(fat);
        free(dir);
}

/* file system management */
int make_fs(char* disk_name)
{
        /* disk */
        if( make_disk(disk_name) ) { return -1; }
        if( open_disk(disk_name) ) { return -1; }

        /* write/initialise the necessary meta-information */
        fs.fatIdx = 1;
        fs.fatLen = (BLOCKS_TOTAL * sizeof(int) + BLOCK_SIZE - 1) / BLOCK_SIZE;

        fs.dirIdx = fs.fatIdx + fs.fatLen;
        fs.dirLen = (MAX_FILES * sizeof(struct dirEntry) + BLOCK_SIZE - 1) / BLOCK_SIZE;

        fs.dataIdx = fs.dirIdx + fs.dirLen;

        char buf[BLOCK_SIZE];

        memcpy(buf, &fs, sizeof(fs));
        if( block_write(0, buf) ) { return -1; }

        fsInit();

        int ii = 0;

        for( ii = 0; ii < fs.fatLen; ii++ )
        {
                memcpy(buf, fat + ii * BLOCK_SIZE / sizeof(int), BLOCK_SIZE);
                if( block_write(fs.fatIdx + ii, buf) ) { return -1; }
        }

        for( ii = 0; ii < fs.dirLen; ii++ )
        {
                memcpy(buf, dir + ii * BLOCK_SIZE / sizeof(struct dirEntry), BLOCK_SIZE);
                if( block_write(fs.dirIdx + ii, buf) ) { return -1; }
        }

        if( close_disk() ) { return -1; }

        fsExit();

        return 0;
}

int mount_fs(char* disk_name)
{
        if( open_disk(disk_name) ) { return -1; }

        char buf[BLOCK_SIZE];
        if( block_read(0, buf) ) { return -1; }
        memcpy(&fs, buf, sizeof(fs));

        fsInit();

        int ii = 0;

        for( ii = 0; ii < fs.fatLen; ii++ )
        {
                if( block_read(fs.fatIdx + ii, buf) ) { return -1; }
                memcpy(fat + ii * (BLOCK_SIZE / sizeof(int)), buf, BLOCK_SIZE);
        }

        for( ii = 0; ii < fs.dirLen; ii++ )
        {
                if( block_read(fs.dirIdx + ii, buf) ) { return -1; }
                memcpy(dir + ii * (BLOCK_SIZE / sizeof(struct dirEntry)), buf, BLOCK_SIZE);
        }

        return 0;
}

int umount_fs(char* disk_name)
{
        int ii = 0;

        char buf[BLOCK_SIZE];

        for( ii = 0; ii < fs.fatLen; ii++)
        {
                memcpy(buf, fat + ii * (BLOCK_SIZE / sizeof(int)), BLOCK_SIZE);
                if( block_write(fs.fatIdx + ii, buf) ) { return -1; }
        }

        for( ii = 0; ii < fs.dirLen; ii++)
        {
                memcpy(buf, dir + ii * (BLOCK_SIZE / sizeof(struct dirEntry)), BLOCK_SIZE);
                if( block_write(fs.dirIdx + ii, buf) ) { return -1; }
        }

        if( close_disk() ) { return -1; }

        fsExit();

        return 0;
}

/* file system functions */
int fs_open(char* name)
{
        if( strlen(name) == 0 || strlen(name) > MAX_FILENAME ) { return -1; }

        int ii = 0;

        for( ii = 0; ii < MAX_FILES; ii++ )
        {
                if( dir[ii].used && !strncmp(dir[ii].name, name, MAX_FILENAME + 1) )
                {
                        int jj = 0;
                        for( jj = 0; jj < MAX_FILDES; jj++ )
                        {
                                if( !fds[jj].used && fds[jj].file != -1 )
                                {
                                        fds[jj].used = 1;
                                        fds[jj].file = ii;
                                        fds[jj].offset = 0;
                                        dir[ii].refCount++;

                                        return jj;
                                }
                        }
                break;
                }
        }

        return -1;
}

int fs_close(int fildes)
{
        if( fildes < 0 || fildes >= MAX_FILDES || !fds[fildes].used ) { return -1; }

        dir[fds[fildes].file].refCount--;
        fds[fildes].used = 0;
        fds[fildes].file = -1;

        return 0;
}

int fs_create(char* name)
{
        if( strlen(name) <= 0 || strlen(name) > MAX_FILENAME ) { return -1; }

        if( numFiles >= MAX_FILES ) { return -1; }

        int ii = 0;

        for( ii = 0; ii < MAX_FILES; ii++ )
        {
                if( dir[ii].used && !strncmp(dir[ii].name, name, strlen(name)) ) { return -1; }

                if( !dir[ii].used )
                {
                        strncpy(dir[ii].name, name, MAX_FILENAME);
                        dir[ii].name[MAX_FILENAME] = '\0';
                        dir[ii].used = 1;
                        dir[ii].size = 0;
                        dir[ii].head = -1;
                        dir[ii].refCount = 0;
                        break;
                }
        }
        numFiles++;
        return 0;
}

int fs_delete(char* name)
{
        int ii = 0;

        if( dir[ii].used == 0 ) { return -1; }

        if( numFiles <= 0 ) { return -1; }

        for( ii = 0; ii < MAX_FILES; ii++ )
        {
                if( dir[ii].used && !strcmp(dir[ii].name, name) )
                {
                        if( dir[ii].refCount > 0 ) { return -1; }

                        int block = dir[ii].head;

                        while( block != -1 )
                        {
                                int next = fat[block];
                                fat[block] = 0;
                                block = next;
                        }

                        memset(&dir[ii], 0, sizeof(dir[ii]));
                        numFiles--;
                        return 0;
                }
        }

        return -1;
}

int fs_read(int fildes, void *buf, size_t nbyte)
{
        if( fildes < 0 || fildes >= MAX_FILDES || !fds[fildes].used ) { return -1; }

        struct dirEntry *file = &dir[fds[fildes].file];
        size_t bytesRead = 0, bytesToRead = nbyte, offset = fds[fildes].offset;
        int block = file->head;
        if( offset >= (size_t)file->size ) { return 0; } /* at EOF */

        if( offset + nbyte > (size_t)file->size ) { bytesToRead = (size_t)file->size - offset; }

        while( offset >= BLOCK_SIZE )
        {
                if( block == -1 ) { return -1; }

                block = fat[block];
                offset -= BLOCK_SIZE;
        }

        char blockBuffer[BLOCK_SIZE];

        while( bytesToRead > 0 && block != -1 )
        {
                if( block_read(fs.dataIdx + block, blockBuffer) == -1 ) { return -1; }

                size_t toCopy = (BLOCK_SIZE - offset < bytesToRead) ? (BLOCK_SIZE - offset) : bytesToRead;
                memcpy((char*)buf + bytesRead, blockBuffer + offset, toCopy);

                bytesRead += toCopy;
                bytesToRead -= toCopy;
                offset = 0;
                block = fat[block];
        }

        fds[fildes].offset += bytesRead;

        return (int)bytesRead;

}

int fs_write(int fildes, void *buf, size_t nbyte)
{
        if( fildes < 0 || fildes >= MAX_FILDES || !fds[fildes].used ) { return -1; }

        int fileIndex = fds[fildes].file;
        struct dirEntry *file = &dir[fileIndex];
        size_t bytesToWrite = nbyte;
        size_t bytesWritten = 0;

        int block = file->head;
        int prevBlock = -1;
        size_t blockOffset = fds[fildes].offset % BLOCK_SIZE;
        int currentBlock = fds[fildes].offset / BLOCK_SIZE;

        int ii = 0;

        for( ii = 0; ii < currentBlock; ii++ )
        {
                prevBlock = block;
                block = fat[block];

                if( block == -1 )
                {
                        for( block = 0; block < BLOCKS_DATA; block++ )
                        {
                                if( !fat[block] ) { break; }
                        }

                        if (block == BLOCKS_DATA) { return bytesWritten; }

                        fat[block] = -1;

                        if (prevBlock != -1)
                        {
                                fat[prevBlock] = block;
                        }
                        else
                        {
                                file->head = block;
                        }
                }
        }

        char blockBuffer[BLOCK_SIZE];

        while( bytesToWrite > 0 )
        {
                if( block_read(fs.dataIdx + block, blockBuffer) ) { return -1; }

                size_t toCopy = (BLOCK_SIZE - blockOffset < bytesToWrite) ? (BLOCK_SIZE - blockOffset) : (bytesToWrite);
                memcpy(blockBuffer + blockOffset, (char *)buf + bytesWritten, toCopy);

                if( block_write(fs.dataIdx + block, blockBuffer) ) { return -1; }

                bytesWritten += toCopy;
                bytesToWrite -= toCopy;
                blockOffset = 0;

                if( bytesToWrite > 0 )
                {
                        prevBlock = block;
                        block = fat[block];

                        if( block == -1 )
                        {
                                for( block = 0; block < BLOCKS_DATA; block++ )
                                {
                                        if( !fat[block] ) { break; }
                                }

                                if (block == BLOCKS_DATA) { return bytesWritten; }

                                fat[block] = -1;
                                fat[prevBlock] = block;
                        }
                }
        }

        fds[fildes].offset += bytesWritten;
        file->size += bytesWritten;
        if (fds[fildes].offset > file->size) { file->size = fds[fildes].offset; }

        return bytesWritten;
}

int fs_get_filesize(int fildes)
{
        if( fildes < 0 || fildes >= MAX_FILDES || !fds[fildes].used ) { return -1; }

        return dir[fds[fildes].file].size;
}


int fs_listfiles(char ***files)
{
        if( !files ) { return -1; }

        int fileCount = 0, ii = 0;
        for( ii = 0; ii < MAX_FILES; ii++ )
        {
                if (dir[ii].used) { fileCount++; }
        }

        *files = malloc((fileCount + 1) * sizeof(char *));
        if (*files == NULL) { return -1; }

        int index = 0;
        for( ii = 0; ii < MAX_FILES; ii++ )
        {
                if (dir[ii].used)
                {
                        (*files)[index] = strdup(dir[ii].name);
                        if ((*files)[index] == NULL)
                        {
                                int jj = 0;
                                for( jj = 0; jj < index; jj++ ) { free((*files)[jj]); }
                                free(*files);
                                return -1;
                        }
                        index++;
                }
        }

        (*files)[index] = NULL;

        return 0;
}

int fs_lseek(int fildes, off_t offset)
{
        if( fildes < 0 || fildes >= MAX_FILDES || !fds[fildes].used ) { return -1; }

        if( offset < 0 || offset > dir[fds[fildes].file].size ) { return -1; }

        fds[fildes].offset = offset;

        return 0;
}
int find_free_block()
{
        int ii = 0;

        for( ii = 0; ii < BLOCKS_DATA; ii++ )
        {
                if( !fat[ii] ) { return ii; }
        }

        return -1;
}

int fs_truncate(int fildes, off_t length)
{
        if( fildes < 0 || fildes >= MAX_FILDES || !fds[fildes].used ) { return -1; }

        if( length < 0 ) { return -1; }

        struct dirEntry *file = &dir[fds[fildes].file];

        if( (size_t)length > (size_t)BLOCKS_DATA * BLOCK_SIZE ) { return -1; }

        int currentSize = file->size;
        int currentBlock = file->head;
        int prevBlock = -1;

        if( length > currentSize )
        {
                char blockBuffer[BLOCK_SIZE] = {0};
                int offset = currentSize;

                while( offset >= BLOCK_SIZE && currentBlock != -1 )
                {
                        prevBlock = currentBlock;
                        currentBlock = fat[currentBlock];
                        offset -= BLOCK_SIZE;
                }

                while( currentSize < length )
                {
                        if (currentBlock == -1)
                        {
                                int newBlock = find_free_block();
                                if( newBlock == -1 ) { return -1; }
                                if (prevBlock != -1)  { fat[prevBlock] = newBlock; }
                                else { file->head = newBlock; }
                                fat[newBlock] = -1;
                                currentBlock = newBlock;
                        }

                        if( block_write(fs.dataIdx + currentBlock, blockBuffer) == -1 )  { return -1; }

                        prevBlock = currentBlock;
                        currentBlock = fat[currentBlock];
                        currentSize += BLOCK_SIZE;
                }
        }

        if( length < currentSize )
        {
                int block = file->head;
                int offset = length;

                while( offset >= BLOCK_SIZE && block != -1 )
                {
                        prevBlock = block;
                        block = fat[block];
                        offset -= BLOCK_SIZE;
                }

                if( prevBlock != -1 ) { fat[prevBlock] = -1; }

                while( block != -1 )
                {
                        int nextBlock = fat[block];
                        fat[block] = 0;
                        block = nextBlock;
                }
        }
        file->size = length;

        return 0;
}
                
