#include <asm-generic/errno-base.h>
#define FUSE_USE_VERSION 35
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "libmfs.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CHK(stmt) assert(stmt != -1);

int translate(const char *path) {
  char *str = strdup(path);
  char *pch;
  pch = strtok(str, "/");
  int inum = 0;
  while (inum >= 0 && pch != NULL) {
    inum = MFS_Lookup(inum, pch);
    pch = strtok(NULL, "/");
  }
  free(str);
  return inum;
}

static int do_getattr(const char *path, struct stat *st) {
  int inum = translate(path);
  if (inum < 0)
    return -ENOENT;
  st->st_uid = getuid();
  st->st_gid = getgid();
  st->st_atime = time(NULL);
  st->st_mtime = time(NULL);
  MFS_Stat_t m;
  int ret = MFS_Stat(inum, &m);
  CHK(ret);
  st->st_mode = m.type == MFS_REGULAR_FILE ? S_IFREG | 0644 : S_IFDIR | 0755;
  st->st_nlink = m.type == MFS_REGULAR_FILE ? 1 : 2;
  st->st_size = m.size;
  return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi) {
  printf("[readdir] %s\n", path);
  int inum = translate(path);
  char block[MFS_BLOCK_SIZE] = {};

  for (int i = 0; i < MFS_MAX_DPTR; i++) {
    int ret = MFS_Read(inum, block, i);
    CHK(ret);
    if (ret == 0)
      continue;
    char *ptr = block;
    while (ptr - block < MFS_BLOCK_SIZE) {
      MFS_DirEnt_t *dent = (MFS_DirEnt_t *)ptr;
      if (dent->inum != -1)
        filler(buffer, dent->name, NULL, 0);
      ptr += sizeof(MFS_DirEnt_t);
    }
  }
  return 0;
}

static int do_open(const char *path, struct fuse_file_info *fi) {
  int inum = translate(path);
  if (inum < 0)
    return -ENOENT;
  fi->fh = inum;
  return 0;
}

static int do_read(const char *path, char *buffer, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  printf("[read] %s %lu\n", path, size);
  // TODO: Remove this
  assert(offset % MFS_BLOCK_SIZE == 0);
  char bkbuf[MFS_BLOCK_SIZE] = {};
  int inum = translate(path);
  int cur = offset / MFS_BLOCK_SIZE;
  int cursz = 0;
  if (offset % MFS_BLOCK_SIZE != 0) {
    int ret = MFS_Read(inum, bkbuf, cur);
    CHK(ret);
    int off_in_block = offset % MFS_BLOCK_SIZE;
    memcpy(buffer, bkbuf + off_in_block,
           MIN(size, MFS_BLOCK_SIZE - off_in_block));
    cursz += ret;
    cur++;
  }

  while (cursz < size) {
    int ret = MFS_Read(inum, bkbuf, cur);
    CHK(ret);
    if (ret == 0)
      return cursz;
    memcpy(buffer + cursz, bkbuf, ret);
    cursz += ret;
    cur++;
  }
  return cursz;
}

static int do_write(const char *path, const char *buffer, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
  printf("[write] %s %d %d\n", path, size, offset);
  int inum = translate(path);
  int cur = offset / MFS_BLOCK_SIZE;
  int remain = size;

  if (offset % MFS_BLOCK_SIZE != 0) {
    char bkbuf[MFS_BLOCK_SIZE] = {};
    int ret = MFS_Read(inum, bkbuf, cur);
    CHK(ret);
    int off_in_block = offset % MFS_BLOCK_SIZE;
    int remain_in_block = MIN(MFS_BLOCK_SIZE - off_in_block, size);
    memcpy(bkbuf + off_in_block, buffer, remain_in_block);
    ret = MFS_Write(inum, bkbuf, cur, off_in_block + remain_in_block);
    buffer += remain_in_block;
    remain -= remain_in_block;
    cur += 1;
  }

  while (remain > 0) {
    int write_sz = MIN(remain, MFS_BLOCK_SIZE);
    int ret = MFS_Write(inum, buffer, cur, write_sz);
    CHK(ret);
    buffer += write_sz;
    remain -= write_sz;
    cur += 1;
  }
  return size;
}

static int do_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  char *work = strdup(path);
  char *pos = strrchr(work, '/');
  *pos = 0;
  printf("[create] %s %s\n", work, pos + 1);
  int inum = translate(work);
  int ret = MFS_Creat(inum, MFS_REGULAR_FILE, pos + 1);
  CHK(ret);
  free(work);
  return ret;
}

static int do_mkdir(const char *path, mode_t mode) {
  char *work = strdup(path);
  char *pos = strrchr(work, '/');
  *pos = 0;
  printf("[mkdir] %s %s\n", work, pos + 1);
  int inum = translate(work);
  int ret = MFS_Creat(inum, MFS_DIRECTORY, pos + 1);
  CHK(ret);
  free(work);
  return ret;
}

static int do_unlink(const char *path) {
  char *work = strdup(path);
  char *pos = strrchr(work, '/');
  *pos = 0;
  printf("[unlink] %s %s\n", work, pos + 1);
  int inum = translate(work);
  int ret = MFS_Unlink(inum, pos + 1);
  CHK(ret);
  free(work);
  return ret;
}

void *do_init(struct fuse_conn_info *conn) {
  MFS_Init();
  return 0;
}

static struct fuse_operations operations = {.create = do_create,
                                            .init = do_init,
                                            .getattr = do_getattr,
                                            .readdir = do_readdir,
                                            .read = do_read,
                                            .mkdir = do_mkdir,
                                            .unlink = do_unlink,
                                            .write = do_write,
                                            .open = do_open};

int main(int argc, char *argv[]) {
  return fuse_main(argc, argv, &operations, NULL);
}
