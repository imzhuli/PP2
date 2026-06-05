#include "./rdkafka_wrapper.hpp"

#include <pp_common/service_runtime.hpp>
#include <typeinfo>

/*
TODO
    在大量发送消息后. 进行重置producer, 这里要考虑线程同步. 以及回收旧对象前的flush和超时操作

*/

/* Callback object */

class xKfkDeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    static constexpr const uint64_t AuditTimeoutMS = 10'000;

    struct xAudit {
        uint64_t AuditLastOffset            = 0;
        uint64_t AuditLastSegAverageLatency = 0;  // micro->milli
    };

    void dr_cb(RdKafka::Message & message) {
        auto G = xel::xSpinlockGuard(AuditLock);
        if (message.err()) {
            ++TotalFailure;
        } else {
            ++TotalSuccess;
            ++LastSegMessageCount;
            LastSegTotalLatency  += message.latency();
            Audit.AuditLastOffset = message.offset();
        }
    }

    void Reset() {
        auto G = xel::xSpinlockGuard(AuditLock);
        xel::Reset(TotalSuccess);
        xel::Reset(TotalFailure);
        xel::Reset(Audit.AuditLastOffset);
        xel::Reset(Audit.AuditLastSegAverageLatency);
        xel::Reset(LastSegMessageCount);
        xel::Reset(LastSegTotalLatency);
    }

    void Tick(uint64_t NowMS) {
        LocalTicker.Update(NowMS);
        if (NowMS - LastAuditTimestampMS < AuditTimeoutMS) {
            return;
        }
        do {
            auto G = xel::xSpinlockGuard(AuditLock);
            if (!LastSegMessageCount) {
                Audit.AuditLastSegAverageLatency = 0;
            } else {
                Audit.AuditLastSegAverageLatency = LastSegTotalLatency / LastSegMessageCount / 1000;  // micro -> milli
            }
            xel::Reset(LastSegMessageCount);
            xel::Reset(LastSegTotalLatency);
        } while (false);
        LastAuditTimestampMS = LocalTicker();
    }

    xAudit GetAudit() const {
        auto G = xel::xSpinlockGuard(AuditLock);
        return Audit;
    }

private:
    size_t TotalSuccess = 0;
    size_t TotalFailure = 0;

    // audit
    xTicker LocalTicker;
    xAudit  Audit = {};

    xel::xSpinlock AuditLock            = {};
    uint64_t       LastAuditTimestampMS = 0;
    uint64_t       LastSegMessageCount  = 0;
    uint64_t       LastSegTotalLatency  = 0;
};

/**/

bool xKfkProducer::Init(const std::string & Topic, const std::map<std::string, std::string> & KafkaParams) {
    KfkCB          = new xKfkDeliveryReportCb();
    auto CBCleaner = xScopeGuard([this] { delete Steal(KfkCB); });

    KfkConf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    if (!KfkConf) {
        DEBUG_LOG("Failed to create kafka config");
        return false;
    }
    auto ConfCleaner = xScopeGuard([this] { delete Steal(KfkConf); });

    KfkToipcName          = Topic;
    auto TopicNameCleaner = xScopeGuard([this] { Reset(KfkToipcName); });

    std::string errstr;
    for (auto & [K, V] : KafkaParams) {
        if (RdKafka::Conf::CONF_OK != KfkConf->set(K, V, errstr)) {
            DEBUG_LOG("failed to set kafka producer param: %s -> %s, error=%s", K.c_str(), V.c_str(), errstr.c_str());
            return false;
        }
    }
    if (RdKafka::Conf::CONF_OK != KfkConf->set("dr_cb", KfkCB, errstr)) {
        DEBUG_LOG("failed to set kafka producer param: %s, error=%s", "dr_cb", errstr.c_str());
        return false;
    }

    auto FirstCreate = CreateProducer();
    if (!FirstCreate) {
        DEBUG_LOG("Failed to create producer with topic");
        return false;
    }

    CBCleaner.Dismiss();
    ConfCleaner.Dismiss();
    TopicNameCleaner.Dismiss();

    RuntimeAssert(RunState.Start());
    PollThread = std::thread{ [this] {
        while (RunState) {
            Poll();
        }
    } };

    return true;
}

void xKfkProducer::Clean() {
    RunState.Stop();
    PollThread.join();
    Reset(PollThread);
    RunState.Finish();

    DestroyProducer();
    auto CBCleaner        = xScopeGuard([this] { delete Steal(KfkCB); });
    auto ConfCleaner      = xScopeGuard([this] { delete Steal(KfkConf); });
    auto TopicNameCleaner = xScopeGuard([this] { Reset(KfkToipcName); });
    DEBUG_LOG("done");
}

bool xKfkProducer::CreateProducer() {
    assert(!Producer.KfkProducer && !Producer.KfkTopic);
    Producer = CreateNativeProducer();
    if (!Producer.KfkProducer) {
        return false;
    }
    return true;
}

void xKfkProducer::DestroyProducer() {
    DestroyNativeProducer(std::move(Producer));
}

auto xKfkProducer::CreateNativeProducer() -> xTopicProducer {
    auto Ret    = xTopicProducer{};
    auto errstr = std::string();

    Ret.KfkProducer = RdKafka::Producer::create(KfkConf, errstr);
    if (!Ret.KfkProducer) {
        DEBUG_LOG("error=%s", errstr.c_str());
        return {};
    }
    auto KPG = xScopeGuard([&] { delete Steal(Ret.KfkProducer); });

    Ret.KfkTopic = RdKafka::Topic::create(Ret.KfkProducer, KfkToipcName, NULL, errstr);
    if (!Ret.KfkTopic) {
        DEBUG_LOG("error=%s", errstr.c_str());
        return {};
    }
    auto KTG = xScopeGuard([&] { delete Steal(Ret.KfkTopic); });

    KPG.Dismiss();
    KTG.Dismiss();
    return Ret;
}

void xKfkProducer::DestroyNativeProducer(xTopicProducer && TP) {
    assert(TP.KfkProducer && TP.KfkTopic);
    auto KPG = xScopeGuard([&] { delete Steal(TP.KfkProducer); });
    auto KTG = xScopeGuard([&] { delete Steal(TP.KfkTopic); });
    return;
}

void xKfkProducer::CheckAndRecreateProducer() {
    Pure();
}

bool xKfkProducer::Post(const std::string & Key, const void * DataPtr, const size_t Size) {
    RdKafka::ErrorCode resp = Producer.KfkProducer->produce(
        Producer.KfkTopic,               // 主题对象
        RdKafka::Topic::PARTITION_UA,    // 分区（PARTITION_UA 表示自动分配）
        RdKafka::Producer::RK_MSG_COPY,  // 消息复制策略
        (char *)(DataPtr),               // 消息内容
        Size,                            // 消息长度
        (Key.empty() ? nullptr : &Key),  // 键（可选）
        NULL                             // 消息头（可选）
    );
    if (resp != RdKafka::ERR_NO_ERROR) {
        DEBUG_LOG("failed to post payload");
        return false;
    }
    return true;
}

void xKfkProducer::Flush() {
    if (!Producer.KfkProducer) {
        return;
    }
    Producer.KfkProducer->flush(5'000);
}

void xKfkProducer::Poll(int TimeoutMS) {
    Producer.KfkProducer->poll(TimeoutMS);
    KfkCB->Tick(xel::GetTimestampMS());
}

std::string xKfkProducer::GetAuditOutput() const {
    auto Audit = KfkCB->GetAudit();
    auto OS    = std::ostringstream();
    OS << "AuditLastOffset:" << Audit.AuditLastOffset << endl;
    OS << "AuditLastSegAverageLatency:" << Audit.AuditLastSegAverageLatency << endl;
    return OS.str();
}
