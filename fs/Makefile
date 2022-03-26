
COMPILER = gcc
FILESYSTEM_FILES = libmfs.c libmfs.h mfs.c io.c io.h

build: $(FILESYSTEM_FILES)
	$(COMPILER) $(FILESYSTEM_FILES) -g -o mfs `pkg-config fuse --cflags --libs`

clean:
	rm mfs
