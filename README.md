# dds-bench
Simple tool for benchmarking various OMG-DDS scenarios

## Description
The dds-bench tool makes it easy to evaluate OMG-DDS performance in simple scenario's. A user can provide various settings such as number of instances, samples, topics, partitions and various QoS settings. The example is written using the ISO C++ API.

## Building
A simple `build.sh` file is included that builds the project. Note that the build.sh file is written for [OpenSplice](https://github.com/PrismTech/opensplice).

## Usage
### Publisher
 pub [--samples n] [--instances n] [--topics n] [--partitions n] [--type simple|payload] [--\<qos\>] [--help]
 
   --partitions:    set the number of partitions the test creates
   
   --topics:        set the number of topics the test creates
   
   --instances:     set the number of instances per topic the test will create
   
   --samples:       set the number of samples per instance the test creates
   
 qos settings:
 
   --latencybudget: set the latency budget to 100 msec
   
   --noreliable:    disable Reliability (default is enabled)
   
   --transient:     set durability to Transient (default is Volatile)
   
   --persistent:    set durability to Persistent
   
   --keepall:       set the history to KeepAll (default is KeepLast)
   
   --type:          set the topic type
   
### Subscriber
Not yet included.
