#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "perftest_DCPS.hpp"

// Generic writer writing sets of X samples for Y instances for Z topics
template <class T>
class Writer {
public:
    Writer( 
        unsigned int topics,
        dds::domain::DomainParticipant dp,
        dds::pub::Publisher pub,
        dds::topic::qos::TopicQos qos
    ) :
        m_topicN (topics),
        m_topics (topics, dds::core::null),
        m_writers (topics, dds::core::null) 
    {
        for (int i = 0; i < topics; ++i) {
            std::stringstream ss;
            ss << (i + 1);
            std::string topicName = ss.str();
            m_topics[i] = dds::topic::Topic<T>(dp, "t" + topicName, qos);
            dds::pub::qos::DataWriterQos dwQos = qos;
            m_writers[i] = dds::pub::DataWriter<T>(pub, m_topics[i], dwQos);
        }
    }

    void write(int instances, int samples) {
        T sample;
        std::cout << " >> writing " << instances << " * " << samples << " * " << m_topicN 
                  << " = " 
                  << instances * samples * m_topicN << " samples" << std::endl;
        for (int s = 0; s < samples; ++s) {
            for (int i = 0; i < instances; ++i) {
                sample = getSample(instances, samples);
                for (int w = 0; w < m_topicN; ++w) {
                    m_writers[w] << sample;
                }
            }
        }
    }

protected:
    virtual T getSample(int instanceId, int sampleId) = 0;

private:
    unsigned int m_topicN;
    std::vector< dds::topic::Topic<T> > m_topics;
    std::vector< dds::pub::DataWriter<T> > m_writers;
};

// Overrides Writer::getSample
class SimpleWriter : public Writer<Perftest::Simple> {
public:
    SimpleWriter( 
        unsigned int topics,
        dds::domain::DomainParticipant dp,
        dds::pub::Publisher pub,
        dds::topic::qos::TopicQos qos
    ) : Writer<Perftest::Simple> (topics, dp, pub, qos) {}

    Perftest::Simple getSample(int instanceId, int sampleId) {
        return Perftest::Simple(instanceId, sampleId);
    }
};

// Create publisher for X partitions
template <class T>
class Publisher {
public:
    Publisher(
        unsigned int samples, 
        unsigned int instances, 
        unsigned int topics, 
        unsigned int partitions,
        dds::domain::DomainParticipant dp,
        dds::topic::qos::TopicQos qos
    ) :
        m_samples (samples),
        m_instances (instances),
        m_publisher (dds::core::null)
    {
        dds::pub::qos::PublisherQos pubQos = dp.default_publisher_qos();
        for (int i = 0; i < partitions; ++i) {
            std::stringstream ss;
            ss << (i + 1);
            std::string partitionName = ss.str();
            pubQos << dds::core::policy::Partition("p" + partitionName);
        }
        m_publisher = dds::pub::Publisher(dp, pubQos);
        m_writer.push_back( T(topics, dp, m_publisher, qos) );
    }

    void write() {
        m_writer[0].write(m_instances, m_samples);
    }
private:
    unsigned int m_instances;
    unsigned int m_samples;
    dds::pub::Publisher m_publisher;
    std::vector<T> m_writer;
};

void printUsage() {
    std::cout << "Usage:" << std::endl <<
         " pub [--samples n] [--instances n] [--topics n] [--partitions n] [--type simple|payload] [--<qos>] [--help]" << std::endl <<
         "   --partitions:    set the number of partitions the test creates" << std::endl <<
         "   --topics:        set the number of topics the test creates" << std::endl <<
         "   --instances:     set the number of instances per topic the test will create" << std::endl << 
         "   --samples:       set the number of samples per instance the test creates" << std::endl <<
         " qos settings:" << std::endl <<
         "   --latencybudget: set the latency budget to 100 msec" << std::endl <<
         "   --noreliable:    disable Reliability (default is enabled)" << std::endl <<
         "   --transient:     set durability to Transient (default is Volatile)" << std::endl <<
         "   --persistent:    set durability to Persistent" << std::endl <<
         "   --keepall:       set the history to KeepAll (default is KeepLast)" << std::endl <<
         "   --type:          set the topic type" << std::endl;
}

typedef enum CmdLineParse {
    ParseNothing,
    ParseSamples,
    ParseInstances,
    ParseTopics,
    ParsePartitions
} CmdLineParse;

typedef enum Type {
    Simple,
    Payload
} Type;

int main(int argc, char *argv[])
{
    int i;
    int instances = 100, samples = 1000, topics = 1, partitions = 1;
    bool run = true;
    bool enableLatencyBudget = false, enableReliability = true, enableKeepall = false;
    dds::core::policy::Durability dQos = dds::core::policy::Durability::Volatile();
    CmdLineParse parsing = ParseNothing;
    Type t = Simple;

    for (i = 0; i < argc; ++i) {
        if (parsing == ParseNothing) {
            if (!strcmp(argv[i], "--samples") || !strcmp(argv[i], "-s")) {
                parsing = ParseSamples;
            } else if (!strcmp(argv[i], "--instances") || !strcmp(argv[i], "-i")) {
                parsing = ParseInstances;
            } else if (!strcmp(argv[i], "--topics") || !strcmp(argv[i], "-t")) {
                parsing = ParseTopics;
            } else if (!strcmp(argv[i], "--partitions") || !strcmp(argv[i], "-p")) {
                parsing = ParsePartitions;
            } else if (!strcmp(argv[i], "--latencybudget") || !strcmp(argv[i], "-l")) {
                enableLatencyBudget = true;
            } else if (!strcmp(argv[i], "--noreliable") || !strcmp(argv[i], "-r")) {
                enableReliability = false;
            } else if (!strcmp(argv[i], "--transient") || !strcmp(argv[i], "-r")) {
                dQos = dds::core::policy::Durability::Transient();
            } else if (!strcmp(argv[i], "--persistent") || !strcmp(argv[i], "-r")) {
                dQos = dds::core::policy::Durability::Persistent();
            } else if (!strcmp(argv[i], "--keepall") || !strcmp(argv[i], "-r")) {
                enableKeepall = true;
            } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
                printUsage();
                run = false;
            }
        } else {
            switch(parsing) {
            case ParseNothing:
                std::cout << "invalid arguments" << std::endl;
                printUsage();
                run = false;
            case ParseSamples:
                samples = atoi(argv[i]);
                break;
            case ParseInstances:
                instances = atoi(argv[i]);
                break;
            case ParseTopics:
                topics = atoi(argv[i]);
                break;
            case ParsePartitions:
                partitions = atoi(argv[i]);
                break;
            }
            parsing = ParseNothing;
        }
    }

    if ((parsing != ParseNothing) || !(instances && samples)) {
        std::cout << "invalid arguments" << std::endl;
        printUsage();
        run = false;
    }

    if (run) {
        std::cout << "Publisher: "
                  << "instances = "  << instances << ", "
                  << "samples = "    << samples << ", "
                  << "topics = "     << topics << ", "
                  << "partitions = " << partitions << "." << std::endl;

        std::cout << "qos" << std::endl;
        if (enableReliability) {
            std::cout << " ├── reliability = Reliable" << std::endl;
        } else {
            std::cout << " ├── reliability = BestEffort" << std::endl;
        }
        if (enableLatencyBudget) {
            std::cout << " ├── latency_budget = 100ms" << std::endl;
        } else {
            std::cout << " ├── latency_budget = 0s" << std::endl;
        }

        if (dQos == dds::core::policy::Durability::Volatile()) {
            std::cout << " ├── durability = Volatile" << std::endl;
        } else if (dQos == dds::core::policy::Durability::Transient()) {
            std::cout << " ├── durability = Transient" << std::endl;
        } else if (dQos == dds::core::policy::Durability::Persistent()) {
            std::cout << " ├── durability = Persistent" << std::endl;
        }

        if (enableKeepall) {
            std::cout << " └── history = KeepAll" << std::endl;
        } else {
            std::cout << " └── history = KeepLast" << std::endl;
        }
        std::cout << std::endl;

        dds::domain::DomainParticipant dp = org::opensplice::domain::default_id() ;
        dds::topic::qos::TopicQos qos = dp.default_topic_qos() 
          << dds::core::policy::Durability::Volatile()
          << dds::core::policy::DestinationOrder::SourceTimestamp()
          << dQos;

        if (enableLatencyBudget) {
            qos << dds::core::policy::LatencyBudget(dds::core::Duration(0, 100000000));
        }
        if (enableReliability) {
            qos << dds::core::policy::Reliability::Reliable();
        }
        if (enableKeepall) {
            qos << dds::core::policy::History::KeepAll();
        }

        std::cout << " >> creating entities" << std::endl;
        Publisher<SimpleWriter> pub(samples, instances, topics, partitions, dp, qos);
        std::cout << " >> start publishing" << std::endl;
        pub.write();
        std::cout << " >> done" << std::endl;
    }

    return 0;
}
