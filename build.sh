idlpp -lisocpp -S perftest.idl -d gen
g++ pub_impl.cpp gen/*.cpp -I gen -g -m32 -I$OSPL_HOME/include -I$OSPL_HOME/include/sys -I$OSPL_HOME/include/dcps/C++/isocpp -I$OSPL_HOME/include/dcps/C++/SACPP -L$OSPL_HOME/lib -ldcpsisocpp -lddskernel -o pub
