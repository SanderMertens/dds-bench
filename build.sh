set -x
gcc src/*.c -g -O3 -Iinclude -lpthread -ldl -o ddsbench
cd ospl
sh build.sh
cd ..
cd lite
sh build.sh
cd ..
