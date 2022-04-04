#!/bin/bash
if [ $# -ge 1 && $1 == "debug" ]; then 
    pushd debuild
    DIR="debuild"
else 
    pushd build
    DIR="build"
fi
echo $DIR
# cmake ..
make -j

if [ $? -eq 0 ]; then
    parallel-scp -h ~/hosts.txt server client mfsfuse ~/sven/$DIR
fi

popd