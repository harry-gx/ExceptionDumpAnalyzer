#ifndef ZLG_CAN_DRIVER_H
#define ZLG_CAN_DRIVER_H

#include "can/can_driver.h"

#include <QLibrary>

#ifndef WIN32
#define WIN32
#endif
#include "zlgcan.h"

class ZlgCanDriver : public ICanDriver
{
public:
    ZlgCanDriver();
    ~ZlgCanDriver() override;

    QString driverName() const override;
    QString libraryPath() const override;
    bool isLoaded() const override;
    bool open(const CanDriverConfig &config, QString *message, QString *error) override;
    void close() override;
    bool transmit(const CanDriverFrame &frame, QString *error) override;
    int receive(CanDriverFrame *frames, int maxFrames, int waitMs, QString *error) override;
    void clearBuffer() override;

private:
    typedef DEVICE_HANDLE(FUNC_CALL *FnOpenDevice)(UINT, UINT, UINT);
    typedef UINT(FUNC_CALL *FnCloseDevice)(DEVICE_HANDLE);
    typedef UINT(FUNC_CALL *FnGetDeviceInfoEx)(DEVICE_HANDLE, ZCAN_DEVICE_INFO_EX *);
    typedef CHANNEL_HANDLE(FUNC_CALL *FnInitCAN)(DEVICE_HANDLE, UINT, ZCAN_CHANNEL_INIT_CONFIG *);
    typedef UINT(FUNC_CALL *FnStartCAN)(CHANNEL_HANDLE);
    typedef UINT(FUNC_CALL *FnResetCAN)(CHANNEL_HANDLE);
    typedef UINT(FUNC_CALL *FnClearBuffer)(CHANNEL_HANDLE);
    typedef UINT(FUNC_CALL *FnGetReceiveNum)(CHANNEL_HANDLE, BYTE);
    typedef UINT(FUNC_CALL *FnTransmit)(CHANNEL_HANDLE, ZCAN_Transmit_Data *, UINT);
    typedef UINT(FUNC_CALL *FnTransmitFD)(CHANNEL_HANDLE, ZCAN_TransmitFD_Data *, UINT);
    typedef UINT(FUNC_CALL *FnReceive)(CHANNEL_HANDLE, ZCAN_Receive_Data *, UINT, int);
    typedef UINT(FUNC_CALL *FnReceiveFD)(CHANNEL_HANDLE, ZCAN_ReceiveFD_Data *, UINT, int);
    typedef UINT(FUNC_CALL *FnSetValue)(DEVICE_HANDLE, const char *, const void *);

    bool resolveApi(QString *error);
    bool setChannelValue(const QString &key, const QByteArray &value, QString *error);
    QString formatDeviceInfo(const ZCAN_DEVICE_INFO_EX &info) const;

    FnOpenDevice m_OpenDevice = nullptr;
    FnCloseDevice m_CloseDevice = nullptr;
    FnGetDeviceInfoEx m_GetDeviceInfoEx = nullptr;
    FnInitCAN m_InitCAN = nullptr;
    FnStartCAN m_StartCAN = nullptr;
    FnResetCAN m_ResetCAN = nullptr;
    FnClearBuffer m_ClearBuffer = nullptr;
    FnGetReceiveNum m_GetReceiveNum = nullptr;
    FnTransmit m_Transmit = nullptr;
    FnTransmitFD m_TransmitFD = nullptr;
    FnReceive m_Receive = nullptr;
    FnReceiveFD m_ReceiveFD = nullptr;
    FnSetValue m_SetValue = nullptr;

    QLibrary m_lib;
    QString m_libraryPath;
    CanDriverConfig m_config;
    DEVICE_HANDLE m_device = INVALID_DEVICE_HANDLE;
    CHANNEL_HANDLE m_channel = INVALID_CHANNEL_HANDLE;
    bool m_opened = false;
};

#endif
