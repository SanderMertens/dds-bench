set -x
dds_idlc -d idl idl/ddsbench.idl
gcc src/*.c idl/*.c -g -I$LITE_HOME/include -Iinclude -I../include -L$LITE_HOME/lib/linux_gcc_x86 -ldds -m64 -pthread -pipe -Wall -fno-strict-aliasing -O3 -std=c99 -Wstrict-prototypes -Dos_linux_gcc_x86 -DLITE=1 -DNDEBUG -D_GNU_SOURCE -fPIC -shared -o liblite.so
