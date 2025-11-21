#include <pp_common/_.hpp>
#include <pp_common/_common.hpp>

class xServerListRegister final : xel::xNonCopyable {
public:
    bool Init(xel::xIoContext * IC);
    void Clean();
    void Tick(uint64_t NowMS);

    void UpdateMasterServierListAddress(const xNetAddress & MasterServierListAddress) { ClientWrapper.UpdateTarget(MasterServierListAddress); }
    void UpdateMyServiceInfo(eServiceType ServiceType, const xServiceInfo & ServiceInfo) {
        assert(ServiceType != eServiceType::ServerIdCenter);
        MyServiceType      = ServiceType;
        MyServiceInfo      = ServiceInfo;
        MyServiceInfoDirty = true;
    }

    eServiceType GetMyServiceType() const { return MyServiceType; }
    xServiceInfo GetMyServiceInfo() const { return MyServiceInfo; }

private:
    void OnConnected();
    void PostRegisterServiceInfo();

private:
    xClientWrapper ClientWrapper;

    eServiceType MyServiceType;
    xServiceInfo MyServiceInfo;
    bool         MyServiceInfoDirty;
};
