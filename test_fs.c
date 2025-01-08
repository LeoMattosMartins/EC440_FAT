#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "fs.h"

#define TEST_DISK "test_disk.img"

void test4f() {
    printf("Running test4f: Open same file 2 times, close fd 1, read from that file using fd 2...\n");
    assert(mount_fs(TEST_DISK) == 0);

    // Create and open the file twice
    assert(fs_create("testfile.txt") == 0);
    int fd1 = fs_open("testfile.txt");
    int fd2 = fs_open("testfile.txt");
    assert(fd1 >= 0 && fd2 >= 0 && fd1 != fd2);

    // Write some data using fd1
    char write_data[] = "Hello, world!";
    assert(fs_write(fd1, write_data, strlen(write_data)) == (int)strlen(write_data));

    // Close fd1 and read using fd2
    assert(fs_close(fd1) == 0);
    char read_data[50];
    assert(fs_lseek(fd2, 0) == 0);
    assert(fs_read(fd2, read_data, strlen(write_data)) == (int)strlen(write_data));
    read_data[strlen(write_data)] = '\0';
    assert(strcmp(write_data, read_data) == 0);

    // Cleanup
    assert(fs_close(fd2) == 0);
    assert(fs_delete("testfile.txt") == 0);
    assert(umount_fs(TEST_DISK) == 0);

    printf("Passed test4f\n");
}

void test7() {
    printf("Running TEST 7...\n");
    assert(mount_fs(TEST_DISK) == 0);

    // Create and write a file
    assert(fs_create("testfile.txt") == 0);
    int fd = fs_open("testfile.txt");
    assert(fd >= 0);

    char write_data[2000];
    for (int i = 0; i < 2000; i++) write_data[i] = (char)(i % 256);
    assert(fs_write(fd, write_data, 2000) == 2000);

    // test7a: Read bytes 501 - 510
    char read_data[10];
    assert(fs_lseek(fd, 501) == 0);
    assert(fs_read(fd, read_data, 10) == 10);

    // test7b: Read bytes 900 - 999
    assert(fs_lseek(fd, 900) == 0);
    assert(fs_read(fd, read_data, 100) == 100);

    // test7c: Read bytes 900 - 1999
    char read_data_large[1100];
    assert(fs_lseek(fd, 900) == 0);
    assert(fs_read(fd, read_data_large, 1100) == 1100);

    // test7g: Write 2 bytes at pos 500, verify bytes 500-504
    char overwrite_data[] = "XY";
    assert(fs_lseek(fd, 500) == 0);
    assert(fs_write(fd, overwrite_data, 2) == 2);
    char verify_data[5];
    assert(fs_lseek(fd, 500) == 0);
    assert(fs_read(fd, verify_data, 5) == 5);
    assert(verify_data[0] == 'X' && verify_data[1] == 'Y');

    // test7h: Read 4092 bytes
    char big_read[4092];
    assert(fs_lseek(fd, 0) == 0);
    assert(fs_read(fd, big_read, 4092) == 4092);

    // test7i: Read 8000 bytes
    char huge_read[8000];
    assert(fs_lseek(fd, 0) == 0);
    assert(fs_read(fd, huge_read, 8000) == 8000);

    assert(fs_close(fd) == 0);
    assert(fs_delete("testfile.txt") == 0);
    assert(umount_fs(TEST_DISK) == 0);

    printf("Passed TEST 7\n");
}

void test8() {
    printf("Running TEST 8...\n");
    assert(mount_fs(TEST_DISK) == 0);

    // test8a: Write 10 bytes / read 10 bytes
    assert(fs_create("testfile.txt") == 0);
    int fd = fs_open("testfile.txt");
    assert(fd >= 0);
    char write_data[] = "abcdefghij";
    assert(fs_write(fd, write_data, 10) == 10);
    char read_data[10];
    assert(fs_lseek(fd, 0) == 0);
    assert(fs_read(fd, read_data, 10) == 10);
    assert(strncmp(write_data, read_data, 10) == 0);

    // test8b: Write 10 bytes / read 1 byte at position 5
    char one_byte[1];
    assert(fs_lseek(fd, 5) == 0);
    assert(fs_read(fd, one_byte, 1) == 1);
    assert(one_byte[0] == 'f');

    // test8d: Write 15MB file, write 2MB, only 1MB should succeed
    char large_data[15 * 1024 * 1024];
    memset(large_data, 'A', sizeof(large_data));
    assert(fs_write(fd, large_data, sizeof(large_data)) == 15 * 1024 * 1024);
    char extra_data[2 * 1024 * 1024];
    memset(extra_data, 'B', sizeof(extra_data));
    assert(fs_write(fd, extra_data, sizeof(extra_data)) == 1024 * 1024); // Only 1MB should succeed

    // test8g: Write 1MB data, overwrite bytes 500-600, read full file
    char megabyte[1024 * 1024];
    memset(megabyte, 'X', sizeof(megabyte));
    assert(fs_write(fd, megabyte, sizeof(megabyte)) == 1024 * 1024);
    char overwrite[] = "OVERWRITE";
    assert(fs_lseek(fd, 500) == 0);
    assert(fs_write(fd, overwrite, sizeof(overwrite)) == sizeof(overwrite));
    char full_read[1024 * 1024];
    assert(fs_lseek(fd, 0) == 0);
    assert(fs_read(fd, full_read, sizeof(full_read)) == 1024 * 1024);

    assert(fs_close(fd) == 0);
    assert(fs_delete("testfile.txt") == 0);
    assert(umount_fs(TEST_DISK) == 0);

    printf("Passed TEST 8\n");
}

void test10a() {
    printf("Running TEST 10a: List all files...\n");
    assert(mount_fs(TEST_DISK) == 0);

    assert(fs_create("file1.txt") == 0);
    assert(fs_create("file2.txt") == 0);
    assert(fs_create("file3.txt") == 0);

    char **files;
    assert(fs_listfiles(&files) == 0);

    int count = 0;
    for (int i = 0; files[i] != NULL; i++) {
        printf("File: %s\n", files[i]);
        free(files[i]);
        count++;
    }
    free(files);
    assert(count == 3);

    assert(fs_delete("file1.txt") == 0);
    assert(fs_delete("file2.txt") == 0);
    assert(fs_delete("file3.txt") == 0);
    assert(umount_fs(TEST_DISK) == 0);

    printf("Passed TEST 10a\n");
}

void test12a() {
    printf("Running TEST 12a: Truncate...\n");
    assert(mount_fs(TEST_DISK) == 0);

    assert(fs_create("testfile.txt") == 0);
    int fd = fs_open("testfile.txt");
    assert(fd >= 0);

    char data[] = "Hello, Truncate!";
    assert(fs_write(fd, data, strlen(data)) == (int)strlen(data));

    // Truncate file to 5 bytes
    assert(fs_truncate(fd, 5) == 0);
    char read_data[10];
    assert(fs_lseek(fd, 0) == 0);
    assert(fs_read(fd, read_data, 10) == 5); // Should only read 5 bytes
    read_data[5] = '\0';
    assert(strcmp(read_data, "Hello") == 0);

    assert(fs_close(fd) == 0);
    assert(fs_delete("testfile.txt") == 0);
    assert(umount_fs(TEST_DISK) == 0);

    printf("Passed TEST 12a\n");
}

int main() {
    make_fs(TEST_DISK);
    test4f();
    test7();
    test8();
    test10a();
    test12a();
    printf("All tests passed!\n");
    return 0;
}
