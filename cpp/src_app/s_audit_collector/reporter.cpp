#include "./reporter.hpp"

#include "../lib_util/rdkafka_wrapper.hpp"

#include <pp_protocol/command.hpp>

// static constexpr size_t MAX_AUDIT_REPORTER_COUNT = 2'0000;

struct xKfkContext {
    xKfkProducer KR                  = {};
    std::string  SecurityProtocol    = {};
    std::string  SaslMechanism       = {};
    std::string  SaslUsername        = {};
    std::string  SaslPassword        = {};
    std::string  BootstrapServerList = {};
    std::string  Topic               = {};
};

bool xTargetCollectReporter::Init(const std::string & ConfigFilename) {
    KfkContext = new xKfkContext();
    auto CL    = xel::xConfigLoader(ConfigFilename);

    CL.Require(KfkContext->SecurityProtocol, "SecurityProtocol");
    CL.Require(KfkContext->SaslMechanism, "SaslMechanism");
    CL.Require(KfkContext->SaslUsername, "SaslUsername");
    CL.Require(KfkContext->SaslPassword, "SaslPassword");
    CL.Require(KfkContext->BootstrapServerList, "BootstrapServerList");
    CL.Require(KfkContext->Topic, "Topic");

    return true;
}

void xTargetCollectReporter::Clean() {
    delete Steal(KfkContext);
}

void xTargetCollectReporter::PostTargetCollect(xTargetCollectNode * Node) {
}
