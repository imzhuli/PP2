#pragma once
#include <librdkafka/rdkafkacpp.h>

#include <map>
#include <pp_common/_.hpp>

class xKfkProducer final {
public:
    bool Init(const std::string & Topic, const std::map<std::string, std::string> & KafkaParams);
    void Clean();

    bool Post(const std::string & Key, const void * DataPtr, const size_t Size);
    bool Post(const std::string & Key, std::string_view Data) { return Post(Key, Data.data(), Data.size()); }
    bool Post(std::string_view Data) { return Post({}, Data.data(), Data.size()); }

    void Flush();

protected:
    bool CreateProducer();
    void DestroyProducer();
    void CheckAndRecreateProducer();

    // run in poll thread:

    struct xTopicProducer {
        RdKafka::Producer * KfkProducer = nullptr;
        RdKafka::Topic *    KfkTopic    = nullptr;
    };

    auto CreateNativeProducer() -> xTopicProducer;
    void DestroyNativeProducer(xTopicProducer && TP);
    void Poll();

private:
    RdKafka::Conf * KfkConf = nullptr;
    std::string     KfkToipcName;

    // when post message count reaches very big, switch to a new producer
    xRunState      RunState;
    xTopicProducer Producer;
    xTopicProducer BackupProducer;
    std::thread    PollThread;
};