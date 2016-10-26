idlpp -lc -S idl/ddsbench.idl -d idl
gcc src/*.c idl/*.c -g -Iinclude -Iidl -I$OSPL_HOME/include -I$OSPL_HOME/include/sys -I$OSPL_HOME/include/dcps/C/SAC -L$OSPL_HOME/lib -ldcpssac -lddskernel -lpthread -o ddsbench
