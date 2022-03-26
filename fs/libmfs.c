#include "libmfs.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WRITE(fd, buf, len)                                                    \
  if (write(fd, buf, len) != len) {                                            \
    perror("write");                                                           \
    assert(0);                                                                 \
  }

#define WRITEEND(fd, buf, len)                                                 \
  lseek(fd, 0, SEEK_END);                                                      \
  WRITE(fd, buf, len)                                                          \
  cp->end += len;

#define READ(fd, buf, len)                                                     \
  if (read(fd, buf, len) != len) {                                             \
    perror("read");                                                            \
    assert(0);                                                                 \
  }

#define MAX_INODE (4096)

#define INODE_IN_PIECE (16)
#define MAX_PIECE (MAX_INODE / INODE_IN_PIECE)

char ZBLOCK[MFS_BLOCK_SIZE] = {0};

typedef struct {
  int end;
  int inode_map_ptrs[MAX_PIECE];
} Checkpoint_t;

typedef struct {
  int inode_ptrs[INODE_IN_PIECE];
} Inodemap_t;

typedef struct Inode_t {
  uint8_t type;
  int size;
  int dptrs[MFS_MAX_DPTR];
} Inode_t;

#define MAX_FILE_IN_DBLOCK (MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t))
typedef struct {
  MFS_DirEnt_t dent[MAX_FILE_IN_DBLOCK];
} Dblock_t;

Checkpoint_t *cp;
int fd;

void initfs() {
  memset(cp, -1, sizeof(Checkpoint_t));
  cp->inode_map_ptrs[0] = sizeof(Checkpoint_t);
  int root_off = cp->inode_map_ptrs[0] + sizeof(Inodemap_t);
  cp->end = root_off + sizeof(Inode_t) + sizeof(Dblock_t);
  WRITE(fd, cp, sizeof(Checkpoint_t));
  Inodemap_t *im = malloc(sizeof(Inodemap_t));
  memset(im->inode_ptrs, -1, sizeof(im->inode_ptrs));
  im->inode_ptrs[0] = root_off;
  WRITE(fd, im, sizeof(Inodemap_t));
  free(im);

  Inode_t *root = malloc(sizeof(Inode_t));
  memset(root, -1, sizeof(Inode_t));
  root->type = MFS_DIRECTORY;
  root->size = 2;
  root->dptrs[0] = root_off + sizeof(Inode_t);
  WRITE(fd, root, sizeof(Inode_t));
  free(root);

  Dblock_t dblock;
  memset(&dblock, -1, sizeof(Dblock_t));
  dblock.dent[0].inum = 0;
  strcpy(dblock.dent[0].name, ".");

  dblock.dent[1].inum = 0;
  strcpy(dblock.dent[1].name, "..");

  WRITE(fd, &dblock, sizeof(Dblock_t));
  fsync(fd);
}

void syncfs() {
  lseek(fd, 0, SEEK_SET);
  WRITE(fd, cp, sizeof(Checkpoint_t));
  fsync(fd);
}

Inode_t *finode(int inum) {
  if (inum < 0 && inum >= MAX_INODE)
    return NULL;
  int off = cp->inode_map_ptrs[inum / INODE_IN_PIECE];
  lseek(fd, off, SEEK_SET);
  Inodemap_t im;
  READ(fd, &im, sizeof(Inodemap_t));
  int idx = inum % INODE_IN_PIECE;
  int off2 = im.inode_ptrs[idx];
  if (off2 == -1)
    return NULL;
  lseek(fd, off2, SEEK_SET);
  Inode_t *inode = malloc(sizeof(Inode_t));
  READ(fd, inode, sizeof(Inode_t));
  return inode;
}

void commit_inode(int inum, Inode_t *inode) {
  int off = cp->inode_map_ptrs[inum / INODE_IN_PIECE];
  Inodemap_t im;
  if (off != -1) {
    lseek(fd, off, SEEK_SET);
    READ(fd, &im, sizeof(Inodemap_t));
  } else {
    memset(&im, -1, sizeof(Inodemap_t));
  }

  int idx = inum % INODE_IN_PIECE;
  if (inode) {
    im.inode_ptrs[idx] = cp->end;
    WRITEEND(fd, inode, sizeof(Inode_t));
  } else {
    im.inode_ptrs[idx] = -1;
  }

  cp->inode_map_ptrs[inum / INODE_IN_PIECE] = cp->end;
  WRITEEND(fd, &im, sizeof(Inodemap_t));
}

int findfree() {
  for (int i = 0; i < MAX_PIECE; i++) {
    int off = cp->inode_map_ptrs[i];
    if (off == -1)
      return i * INODE_IN_PIECE;
    lseek(fd, off, SEEK_SET);
    Inodemap_t im;
    READ(fd, &im, sizeof(Inodemap_t));
    for (int j = 0; j < INODE_IN_PIECE; j++) {
      if (im.inode_ptrs[j] == -1) {
        return i * INODE_IN_PIECE + j;
      }
    }
  }
  return -1;
}

int MFS_Lookup(int pinum, char *name) {
  Inode_t *inode = finode(pinum);
  if (inode == NULL || inode->type != MFS_DIRECTORY) {
    free(inode);
    return -1;
  }
  Dblock_t db;
  for (int i = 0; inode->dptrs[i] != -1; i++) {
    lseek(fd, inode->dptrs[i], SEEK_SET);
    READ(fd, &db, sizeof(Dblock_t));
    for (int j = 0; j < MAX_FILE_IN_DBLOCK; j++) {
      if (strcmp(name, db.dent[j].name) == 0) {
        free(inode);
        return db.dent[j].inum;
      }
    }
  }
  free(inode);
  return -1;
}

int MFS_Stat(int inum, MFS_Stat_t *stat) {
  Inode_t *inode = finode(inum);
  if (inode == NULL)
    return -1;
  stat->type = inode->type;
  stat->size = inode->size;
  free(inode);
  return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
  if (strlen(name) > MFS_MAX_NAME)
    return -1;

  Inode_t *pinode = finode(pinum);
  if (pinode->type != MFS_DIRECTORY) {
    free(pinode);
    return -1;
  }

  for (int i = 0; i < MFS_MAX_DPTR; i++) {
    if (pinode->dptrs[i] == -1)
      continue;
    Dblock_t tmpdb;
    lseek(fd, pinode->dptrs[i], SEEK_SET);
    READ(fd, &tmpdb, sizeof(Dblock_t));
    for (int j = 0; j < MAX_FILE_IN_DBLOCK; j++) {
      if (tmpdb.dent[j].inum == -1)
        continue;
      if (strncmp(tmpdb.dent[j].name, name, MFS_MAX_NAME) == 0) {
        free(pinode);
        return 0;
      }
    }
  }

  int free_inum = findfree();
  assert(free_inum != -1);

  Inode_t *cinode = malloc(sizeof(Inode_t));
  memset(cinode, -1, sizeof(Inode_t));
  cinode->type = type;
  cinode->size = 0;
  if (type == MFS_DIRECTORY) {
    cinode->size = 2;
    cinode->dptrs[0] = cp->end;
    Dblock_t tmpdb;
    memset(&tmpdb, -1, sizeof(Dblock_t));
    tmpdb.dent[0].inum = free_inum;
    strcpy(tmpdb.dent[0].name, ".");

    tmpdb.dent[1].inum = pinum;
    strcpy(tmpdb.dent[1].name, "..");
    WRITEEND(fd, &tmpdb, sizeof(Dblock_t));
  }
  commit_inode(free_inum, cinode);
  free(cinode);

  Dblock_t db;
  int eidx = -1;
  for (int i = 0; i < MFS_MAX_DPTR; i++) {
    if (pinode->dptrs[i] == -1) {
      pinode->dptrs[i] = cp->end;
      memset(&db, -1, sizeof(Dblock_t));
      eidx = 0;
      break;
    } else {
      lseek(fd, pinode->dptrs[i], SEEK_SET);
      READ(fd, &db, sizeof(Dblock_t));
      for (int j = 0; j < MAX_FILE_IN_DBLOCK; j++) {
        if (db.dent[j].inum == -1) {
          eidx = j;
          pinode->dptrs[i] = cp->end;
          break;
        }
      }
      if (eidx != -1)
        break;
    }
  }

  if (eidx == -1) {
    free(pinode);
    return -1;
  }

  pinode->size += 1;
  db.dent[eidx].inum = free_inum;
  strncpy(db.dent[eidx].name, name, MFS_MAX_NAME);
  WRITEEND(fd, &db, sizeof(Dblock_t));

  commit_inode(pinum, pinode);

  free(pinode);
  syncfs();
  return 0;
}

int MFS_Write(int inum, char *buffer, int block, int size) {
  if (block < 0 || block >= MFS_MAX_DPTR)
    return -1;
  Inode_t *inode = finode(inum);
  if (inode == NULL || inode->type == MFS_DIRECTORY)
    return -1;
  for (int i = 0; i < block; i++) {
    if (inode->dptrs[i] != -1)
      continue;
    inode->dptrs[i] = cp->end;
    WRITEEND(fd, &ZBLOCK, MFS_BLOCK_SIZE);
  }
  inode->size = MFS_BLOCK_SIZE * block + size;
  inode->dptrs[block] = cp->end;
  WRITEEND(fd, buffer, MFS_BLOCK_SIZE);
  commit_inode(inum, inode);
  free(inode);
  syncfs();
  return 0;
}

int MFS_Read(int inum, char *buffer, int block) {
  if (block < 0 || block >= MFS_MAX_DPTR)
    return -1;
  Inode_t *inode = finode(inum);
  if (inode == NULL)
    return -1;
  if (inode->dptrs[block] == -1)
    return 0;
  lseek(fd, inode->dptrs[block], SEEK_SET);
  READ(fd, buffer, MFS_BLOCK_SIZE);
  int size = MFS_BLOCK_SIZE;
  if ((block + 1) * MFS_BLOCK_SIZE > inode->size) {
    size = inode->size % MFS_BLOCK_SIZE;
  }
  free(inode);
  return size;
}

int MFS_Unlink(int pinum, char *name) {
  Inode_t *inode = finode(pinum);
  if (inode->type != MFS_DIRECTORY) {
    free(inode);
    return -1;
  }
  Dblock_t db;
  int foundinum = -1;
  for (int i = 0; i < MFS_MAX_DPTR; i++) {
    if (inode->dptrs[i] == -1)
      continue;
    lseek(fd, inode->dptrs[i], SEEK_SET);
    READ(fd, &db, sizeof(Dblock_t));
    for (int j = 0; j < MAX_FILE_IN_DBLOCK; j++) {
      if (db.dent[j].inum == -1)
        continue;
      if (strncmp(db.dent[j].name, name, MFS_MAX_NAME) == 0) {
        Inode_t *cinode = finode(db.dent[j].inum);
        if (cinode->type == MFS_DIRECTORY && cinode->size != 0) {
          free(inode);
          free(cinode);
          return -1;
        }
        free(cinode);
        foundinum = db.dent[j].inum;
        inode->size -= 1;
        inode->dptrs[i] = cp->end;
        memset(&db.dent[j], -1, sizeof(MFS_DirEnt_t));
        WRITEEND(fd, &db, sizeof(Dblock_t));
        commit_inode(pinum, inode);
        break;
      }
    }
    if (foundinum != -1)
      break;
  }

  free(inode);
  if (foundinum == -1)
    return 0;
  commit_inode(foundinum, NULL);
  syncfs();
  return 0;
}

int MFS_Shutdown() {
  syncfs();
  free(cp);
  exit(0);
}

int MFS_Init() {
  char *img = "./fs.img";
  cp = malloc(sizeof(Checkpoint_t));
  if ((fd = open(img, O_CREAT | O_RDWR | O_EXCL, 0644)) >= 0) {
    initfs();
  } else if (errno == EEXIST) {
    fd = open(img, O_RDWR);
    READ(fd, cp, sizeof(Checkpoint_t));
  } else {
    perror("open");
    assert(0);
  }
  return 0;
}

// int main() {
//     MFS_Init();
//     printf("%d\n", MFS_Lookup(0, "test"));
//     MFS_Stat_t m;
//     printf("%d\n", MFS_Stat(1, &m));

//     // printf("sizeof Checkpoint_t %d\n", sizeof(Checkpoint_t));
//     // printf("sizeof Inodemap_t %d\n", sizeof(Inodemap_t));
//     // printf("sizeof Inode_t %d\n", sizeof(Inode_t));
//     // printf("sizeof MFS_DirEnt_t %d\n", sizeof(MFS_DirEnt_t));
//     // printf("sizeof Dblock_t %d\n", sizeof(Dblock_t));
// }
