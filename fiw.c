/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fast Image Writer
 *  -----------------
 *  Copyright (C) 2011, Eduardo Silva P. <edsiper@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>

/* headers for lstat() */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* header for open() */
#include <fcntl.h>

/* header for sendfile() */
#include <sys/sendfile.h>

/* Boolean macros */
#define FALSE  0
#define TRUE   !FALSE

/* user ID */
gid_t egid;
uid_t euid;

struct file_info
{
    off_t size;

    short int is_file;
    short int is_link;
    short int is_char;
    short int is_block;
    short int is_directory;
    short int exec_access;
    short int read_access;
    short int write_access;
    time_t last_modification;
};

int print_err(char *msg)
{
    printf("%s\n\n", msg);
    exit(EXIT_FAILURE);
}

struct file_info *file_get_info(const char *path)
{
    struct stat f, target;
    struct file_info *f_info;

    f_info = malloc(sizeof(struct file_info));

    euid = geteuid();
    egid = getegid();

    /* Stat right resource */
    if (lstat(path, &f) == -1) {
        free(f_info);
        return NULL;
    }

    f_info->is_file = TRUE;
    f_info->is_link = FALSE;
    f_info->is_block = FALSE;
    f_info->is_directory = FALSE;
    f_info->exec_access = FALSE;
    f_info->read_access = FALSE;
    f_info->write_access = FALSE;

    if (S_ISLNK(f.st_mode)) {
        f_info->is_link = TRUE;
        f_info->is_file = FALSE;
        if (stat(path, &target) == -1) {
            return NULL;
        }
    }
    else {
        target = f;
    }

    f_info->size = target.st_size;
    f_info->last_modification = target.st_mtime;

    /* is directory ? */
    if (S_ISDIR(target.st_mode)) {
        f_info->is_directory = TRUE;
        f_info->is_file = FALSE;
    }


    /* is character device ? */
    if (S_ISCHR(target.st_mode)) {
        f_info->is_file = FALSE;
        f_info->is_char = TRUE;
    }

    /* is block device ? */
    if (S_ISBLK(target.st_mode)) {
        f_info->is_file = FALSE;
        f_info->is_block = TRUE;
    }

    /* Checking read access */
    if (((target.st_mode & S_IRUSR) && target.st_uid == euid) ||
        ((target.st_mode & S_IRGRP) && target.st_gid == egid) ||
        (target.st_mode & S_IROTH)) {
        f_info->read_access = TRUE;
    }

    /* Check write access */
    if (((target.st_mode & S_IWUSR) && target.st_uid == euid) ||
        ((target.st_mode & S_IWGRP) && target.st_gid == egid) ||
        (target.st_mode & S_IWOTH)) {
        f_info->write_access = TRUE;
    }

    /* Checking execution access */
    if ((target.st_mode & S_IXUSR && target.st_uid == euid) ||
        (target.st_mode & S_IXGRP && target.st_gid == egid) ||
        (target.st_mode & S_IXOTH)) {
        f_info->exec_access = TRUE;

    }

    return f_info;
}


int main(int argc, char **argv)
{
    int fd_source = 0;
    int fd_target = 0;
    int count = 1024000; /* 1M */
    float progress;
    long bytes, written=0;
    off_t *file_offset = 0;
    struct file_info *f_source;
    struct file_info *f_target;

    if (argc < 3) {
        print_err("Usage: fiw source.img /dev/target_device");
    }

    f_source = file_get_info(argv[1]);
    f_target = file_get_info(argv[2]);


    if (!f_source) {
        print_err("Error: Invalid source");
    }

    if (!f_target) {
        print_err("Error: Invalid target");
    }

    /* Validate read access */
    if (f_source->read_access == FALSE) {
        print_err("Error: I cannot read the source file");
    }

    /* Check that source is a file */
    if (f_source->is_file == FALSE) {
        print_err("Error: source is not a file");
    }

    if (f_target->is_block == FALSE && f_target->is_char == FALSE) {
        print_err("Error: target must be a char or block device");
    }

    if (f_target->write_access == FALSE) {
        print_err("Error: I cannot write to the target block device");
    }

    fd_source = open(argv[1], O_RDONLY);
    if (fd_source == -1) {
        print_err("Error: open() failed on source image file");
    }

    fd_target = open(argv[2], O_WRONLY | O_SYNC);
    if (fd_target == -1) {
        print_err("Error: open() failed on target block device");
    }

    /* Job details */
    printf("Fast Image Writer v0.1\n");
    printf("+ Source file  : %s\n", argv[1]);
    printf("+ Target device: %s\n\n", argv[2]);
    printf("** Progress **\n");
    do {
        bytes = sendfile(fd_target, fd_source, file_offset, count);
        if (bytes > 0) {
            written += bytes;
            progress = ((written * 100.00) / f_source->size);
            printf("\r%2.2f%% (%li/%li bytes)", progress, written, f_source->size);
            fflush(stdout);
        }
                       
    } while (bytes > 0);

    if (bytes < 0) {
        perror("sendfile");
    }

    close(fd_source);
    close(fd_target);

    printf("\n");
    return 0;
}
