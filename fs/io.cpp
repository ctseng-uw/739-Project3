#include "io.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "mfs.h"

#ifdef LOCAL
static int fd;
#else
#include "../BlockStoreClient.cpp"
static std::unique_ptr<BlockStoreClient> client;
#endif

int io_init() {
#ifdef LOCAL
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
#else
  client = std::make_unique<BlockStoreClient>(0, false);
  return 0;
#endif
}

int io_write(int addr, char *buf) {
#ifdef LOCAL
  lseek(fd, addr, SEEK_SET);
  int ret = write(fd, buf, MFS_BLOCK_SIZE);
  fsync(fd);
  return ret;
#else
  client->Write(addr, std::string(buf, MFS_BLOCK_SIZE));
  return MFS_BLOCK_SIZE;
#endif
}

int io_read(int addr, char *buf) {
#ifdef LOCAL
  lseek(fd, addr, SEEK_SET);
  return read(fd, buf, MFS_BLOCK_SIZE);
#else

  auto ret = client->Read(addr);
  memcpy(buf, ret.c_str(), MFS_BLOCK_SIZE);
  return MFS_BLOCK_SIZE;
#endif
}
