#include "io.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "libmfs.h"

static int fd;

int io_init() {
  char *img = "./fs.img";
  if ((fd = open(img, O_CREAT | O_RDWR | O_EXCL, 0644)) >= 0) {
    return 0;
  } else if (errno == EEXIST) {
    fd = open(img, O_RDWR);
    return EEXIST;
  } else {
    perror("open");
    assert(0);
  }
}

int io_write(int addr, char *buf) {
  lseek(fd, addr, SEEK_SET);
  return write(fd, buf, MFS_BLOCK_SIZE);
}

int io_read(int addr, char *buf) {
  lseek(fd, addr, SEEK_SET);
  return read(fd, buf, MFS_BLOCK_SIZE);
}
