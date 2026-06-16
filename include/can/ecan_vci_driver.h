#ifndef ECAN_VCI_DRIVER_H
#define ECAN_VCI_DRIVER_H

#include "can/can_driver.h"

#include <QLibrary>

#include "ECanVci.h"

class ECanVciDriver : public ICanDriver
{
public:
    ECanVciDriver();
    ~ECanVciDriver() override;

    QString driverName() const override;
    QString libraryPath() const override;
    bool isLoaded() const override;
    bool open(const CanDriverConfig &config, QString *message, QString *error) override;
    void close() override;
    bool transmit(const CanDriverFrame &frame, QString *error) override;
    int receive(CanDriverFrame *frames, int maxFrames, int waitMs, QString *error) override;
    void clearBuffer() override;

private:
    bool resolveApi(QString *error);
    QString formatBoardInfo(const BOARD_INFO &info) const;

    typedef DWORD(__stdcall FnOpenDevice)(DWORD, DWORD, DWORD);
    typedef DWORD(__stdcall FnCloseDevice)(DWORD, DWORD);
    typedef DWORD(__stdcall FnInitCAN)(DWORD, DWORD, DWORD, P_INIT_CONFIG);
    typedef DWORD(__stdcall FnStartCAN)(DWORD, DWORD, DWORD);
    typedef ULONG(__stdcall FnTransmit)(DWORD, DWORD, DWORD, P_CAN_OBJ, ULONG);
    typedef ULONG(__stdcall FnReceive)(DWORD, DWORD, DWORD, P_CAN_OBJ, ULONG, INT);
    typedef DWORD(__stdcall FnClearBuffer)(DWORD, DWORD, DWORD);
    typedef DWORD(__stdcall FnReadErrInfo)(DWORD, DWORD, DWORD, P_ERR_INFO);
    typedef DWORD(__stdcall FnReadBoardInfo)(DWORD, DWORD, P_BOARD_INFO);

    FnOpenDevice *m_OpenDevice = nullptr;
    FnCloseDevice *m_CloseDevice = nullptr;
    FnInitCAN *m_InitCAN = nullptr;
    FnStartCAN *m_StartCAN = nullptr;
    FnTransmit *m_Transmit = nullptr;
    FnReceive *m_Receive = nullptr;
    FnClearBuffer *m_ClearBuffer = nullptr;
    FnReadErrInfo *m_ReadErrInfo = nullptr;
    FnReadBoardInfo *m_ReadBoardInfo = nullptr;

    QLibrary m_lib;
    QString m_libraryPath;
    CanDriverConfig m_config;
    bool m_opened = false;
};

#endif
