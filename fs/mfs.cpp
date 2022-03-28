#include "mfs.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "io.h"

int tmp_ret;
#define WRITE(addr, buf)                  \
  tmp_ret = io_write(addr, (char *)buf);  \
  if (tmp_ret == -1) {                    \
    perror("write");                      \
    assert(0);                            \
  } else if (tmp_ret != MFS_BLOCK_SIZE) { \
    printf("write ret: %d\n", tmp_ret);   \
    assert(0);                            \
  }

#define WRITEEND(buf) \
  WRITE(cp->end, buf) \
  cp->end += MFS_BLOCK_SIZE;

#define READ(addr, buf)                   \
  tmp_ret = io_read(addr, (char *)buf);   \
  if (tmp_ret == -1) {                    \
    perror("read");                       \
    assert(0);                            \
  } else if (tmp_ret != MFS_BLOCK_SIZE) { \
    printf("read ret: %d\n", tmp_ret);    \
    assert(0);                            \
  }

#define MAX_INODE (4096)
#define INODE_IN_PIECE (16)
#define MAX_PIECE (MAX_INODE / INODE_IN_PIECE)

char ZBLOCK[MFS_BLOCK_SIZE] = {0};

typedef struct {
  int end;
  int inode_map_ptrs[MAX_PIECE];
  char padding[3068];
} Checkpoint_t;

typedef struct {
  int inode_ptrs[INODE_IN_PIECE];
  char padding[4096 - 64];
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

void initfs() {
  memset(cp, -1, sizeof(Checkpoint_t));
  cp->inode_map_ptrs[0] = sizeof(Checkpoint_t);
  int root_off = cp->inode_map_ptrs[0] + sizeof(Inodemap_t);
  cp->end = root_off + sizeof(Inode_t) + sizeof(Dblock_t);
  WRITE(0, cp);
  Inodemap_t *im = (Inodemap_t *)malloc(sizeof(Inodemap_t));
  memset(im->inode_ptrs, -1, sizeof(im->inode_ptrs));
  im->inode_ptrs[0] = root_off;
  WRITE(MFS_BLOCK_SIZE, im);
  free(im);

  Inode_t *root = (Inode_t *)malloc(sizeof(Inode_t));
  memset(root, -1, sizeof(Inode_t));
  root->type = MFS_DIRECTORY;
  root->size = 2;
  root->dptrs[0] = root_off + sizeof(Inode_t);
  WRITE(MFS_BLOCK_SIZE * 2, root);
  free(root);

  Dblock_t dblock;
  memset(&dblock, -1, sizeof(Dblock_t));
  dblock.dent[0].inum = 0;
  strcpy(dblock.dent[0].name, ".");

  dblock.dent[1].inum = 0;
  strcpy(dblock.dent[1].name, "..");

  WRITE(MFS_BLOCK_SIZE * 3, &dblock);
}

void syncfs() { WRITE(0, cp); }

Inode_t *finode(int inum) {
  if (inum < 0 && inum >= MAX_INODE) return NULL;
  int off = cp->inode_map_ptrs[inum / INODE_IN_PIECE];
  Inodemap_t im;
  READ(off, &im);
  int idx = inum % INODE_IN_PIECE;
  int off2 = im.inode_ptrs[idx];
  if (off2 == -1) return NULL;
  Inode_t *inode = (Inode_t *)malloc(sizeof(Inode_t));
  READ(off2, inode);
  return inode;
}

void commit_inode(int inum, Inode_t *inode) {
  int off = cp->inode_map_ptrs[inum / INODE_IN_PIECE];
  Inodemap_t im;
  if (off != -1) {
    READ(off, &im);
  } else {
    memset(&im, -1, sizeof(Inodemap_t));
  }

  int idx = inum % INODE_IN_PIECE;
  if (inode) {
    im.inode_ptrs[idx] = cp->end;
    WRITEEND(inode);
  } else {
    im.inode_ptrs[idx] = -1;
  }

  cp->inode_map_ptrs[inum / INODE_IN_PIECE] = cp->end;
  WRITEEND(&im);
}

int findfree() {
  for (int i = 0; i < MAX_PIECE; i++) {
    int off = cp->inode_map_ptrs[i];
    if (off == -1) return i * INODE_IN_PIECE;
    Inodemap_t im;
    READ(off, &im);
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
    READ(inode->dptrs[i], &db);
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
  if (inode == NULL) return -1;
  stat->type = inode->type;
  stat->size = inode->size;
  free(inode);
  return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
  if (strlen(name) > MFS_MAX_NAME) return -1;

  Inode_t *pinode = finode(pinum);
  if (pinode->type != MFS_DIRECTORY) {
    free(pinode);
    return -1;
  }

  for (int i = 0; i < MFS_MAX_DPTR; i++) {
    if (pinode->dptrs[i] == -1) continue;
    Dblock_t tmpdb;
    READ(pinode->dptrs[i], &tmpdb);
    for (int j = 0; j < MAX_FILE_IN_DBLOCK; j++) {
      if (tmpdb.dent[j].inum == -1) continue;
      if (strncmp(tmpdb.dent[j].name, name, MFS_MAX_NAME) == 0) {
        free(pinode);
        return 0;
      }
    }
  }

  int free_inum = findfree();
  assert(free_inum != -1);

  Inode_t *cinode = (Inode_t *)malloc(sizeof(Inode_t));
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
    WRITEEND(&tmpdb);
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
      READ(pinode->dptrs[i], &db);
      for (int j = 0; j < MAX_FILE_IN_DBLOCK; j++) {
        if (db.dent[j].inum == -1) {
          eidx = j;
          pinode->dptrs[i] = cp->end;
          break;
        }
      }
      if (eidx != -1) break;
    }
  }

  if (eidx == -1) {
    free(pinode);
    return -1;
  }

  pinode->size += 1;
  db.dent[eidx].inum = free_inum;
  strncpy(db.dent[eidx].name, name, MFS_MAX_NAME);
  WRITEEND(&db);

  commit_inode(pinum, pinode);

  free(pinode);
  syncfs();
  return 0;
}

int MFS_Write(int inum, const char *buffer, int block, int size) {
  if (block < 0 || block >= MFS_MAX_DPTR) return -1;
  Inode_t *inode = finode(inum);
  if (inode == NULL || inode->type == MFS_DIRECTORY) return -1;
  for (int i = 0; i < block; i++) {
    if (inode->dptrs[i] != -1) continue;
    inode->dptrs[i] = cp->end;
    WRITEEND(ZBLOCK);
  }
  inode->size = MFS_BLOCK_SIZE * block + size;
  inode->dptrs[block] = cp->end;
  WRITEEND(buffer);
  commit_inode(inum, inode);
  free(inode);
  syncfs();
  return 0;
}

int MFS_Read(int inum, char *buffer, int block) {
  if (block < 0 || block >= MFS_MAX_DPTR) return -1;
  Inode_t *inode = finode(inum);
  if (inode == NULL) return -1;
  if (inode->dptrs[block] == -1) return 0;
  READ(inode->dptrs[block], buffer);
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
    if (inode->dptrs[i] == -1) continue;
    READ(inode->dptrs[i], &db);
    for (int j = 0; j < MAX_FILE_IN_DBLOCK; j++) {
      if (db.dent[j].inum == -1) continue;
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
        WRITEEND(&db);
        commit_inode(pinum, inode);
        break;
      }
    }
    if (foundinum != -1) break;
  }

  free(inode);
  if (foundinum == -1) return 0;
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
  assert(sizeof(Checkpoint_t) == MFS_BLOCK_SIZE);
  assert(sizeof(Inodemap_t) == MFS_BLOCK_SIZE);
  assert(sizeof(Inode_t) == MFS_BLOCK_SIZE);
  assert(sizeof(Dblock_t) == MFS_BLOCK_SIZE);
  cp = (Checkpoint_t *)malloc(sizeof(Checkpoint_t));
  if (io_init() != EEXIST) {
    initfs();
  }
  READ(0, cp);
  return 0;
}
