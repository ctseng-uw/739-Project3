
COMPILER = g++
FILESYSTEM_FILES = mfs.cpp mfs.h mfsfuse.cpp io.cpp io.h 

build: $(FILESYSTEM_FILES)
	$(COMPILER) $(FILESYSTEM_FILES) -g -DLOCAL -o mfsfuse `pkg-config fuse --cflags --libs`

clean:
	rm mfs
