#include <pp_common/_.hpp>
#include <pp_common/_common.hpp>

class xServerListRegister final : xel::xNonCopyable {
public:
    bool Init(xel::xIoContext * IC);
    void Clean();
    void Tick(uint64_t NowMS);

    void UpdateMasterServierListAddress(const xNetAddress & MasterServierListAddress) { ClientWrapper.UpdateTarget(MasterServierListAddress); }
    void UpdateMyServiceInfo(eServiceType ServiceType, const xServerInfo & ServiceInfo) {
        assert(ServiceType != eServiceType::ServerIdCenter);
        MyServiceInfoDirty = (MyServiceType != ServiceType) && (MyServiceInfo != ServiceInfo);
        MyServiceType      = ServiceType;
        MyServiceInfo      = ServiceInfo;
    }
    void UpdateMyServerId(uint64_t ServerId) {
        MyServiceInfoDirty     = (MyServiceType != eServiceType::Unspecified) && (MyServiceInfo.ServerId != ServerId);
        MyServiceInfo.ServerId = ServerId;
    }

    eServiceType        GetMyServiceType() const { return MyServiceType; }
    const xServerInfo & GetMyServiceInfo() const { return MyServiceInfo; }

    std::function<void(void)> OnError = Shutdown;

private:
    void OnConnected();
    void PostRegisterServiceInfo();

private:
    xClientWrapper ClientWrapper;

    bool         MyServiceInfoDirty = false;
    eServiceType MyServiceType      = eServiceType::Unspecified;
    xServerInfo  MyServiceInfo      = {};
};
