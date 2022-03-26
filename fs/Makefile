
COMPILER = gcc
FILESYSTEM_FILES = libmfs.c libmfs.h mfs.c

build: $(FILESYSTEM_FILES)
	$(COMPILER) $(FILESYSTEM_FILES) -g -o mfs `pkg-config fuse --cflags --libs`

clean:
	rm mfs
