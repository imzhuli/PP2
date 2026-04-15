#pragma once
#include "./device_abstract.hpp"
#include "./pa_abstract.hpp"
#include "./pa_future.hpp"

class xPA_AuthFutureManager : public xFutureManager {
};

class xPA_DeviceFutureManager : public xFutureManager {
};

class xPA_CreateRelayTcpConnectionFeatureManager : public xFutureManager {
};

class xPA_ServiceBase : xAbstract {

protected:
    void BindDeviceService(xDeviceAbstractService * Service) { DeviceService = Service; }
    void ClearTimeoutFuture();

    virtual void ReleaseAuthFuture(uint64_t FutureId);
    virtual void ReleaseCreateRelayTcpConnectionFeature(uint64_t FutureId);

private:
    xTicker                   LocalTicker;
    xPA_AuthFutureManager *   AuthFutureManager;
    xPA_DeviceFutureManager * DeviceFutureManager;
    xDeviceAbstractService *  DeviceService;

    xPA_CreateRelayTcpConnectionFeatureManager * CreateRelayTcpConnectionFeatureManager;
    xPA_CreateRelayTcpConnectionFeatureManager * CreateRelayUdpChannelFeatureManager;

    xFutureList AuthFutureTimeoutList;
    xFutureList CreateRelayTcpConnectionFeatureTimeoutList;
    xFutureList CreateRelayUdpChannelFeatureTimeoutList;
};
