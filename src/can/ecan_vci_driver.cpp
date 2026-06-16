#include "can/ecan_vci_driver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace
{
QString ecanDllName()
{
    return (sizeof(void *) == 8) ? QStringLiteral("ECanVci64.dll") : QStringLiteral("ECanVci.dll");
}

QString resolveEcanVciLibrary()
{
#ifdef Q_OS_WIN
    const QString dllName = ecanDllName();
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;

    candidates << QDir(appDir).absoluteFilePath(dllName)
               << QDir(appDir).absoluteFilePath(QStringLiteral("lib/ECanVci/") + dllName)
               << QDir::current().absoluteFilePath(QStringLiteral("lib/ECanVci/") + dllName);

    QDir walk(appDir);
    for (int depth = 0; depth < 12; ++depth)
    {
        candidates << walk.absoluteFilePath(QStringLiteral("lib/ECanVci/") + dllName);
        if (!walk.cdUp())
        {
            break;
        }
    }

    for (const QString &path : candidates)
    {
        if (QFileInfo::exists(path))
        {
            return QDir::toNativeSeparators(path);
        }
    }
#endif
    return QStringLiteral("ECanVci");
}
}

ECanVciDriver::ECanVciDriver()
    : m_libraryPath(resolveEcanVciLibrary())
{
}

ECanVciDriver::~ECanVciDriver()
{
    close();
}

QString ECanVciDriver::driverName() const
{
    return QStringLiteral("广成科技 ECanVci");
}

QString ECanVciDriver::libraryPath() const
{
    return m_libraryPath;
}

bool ECanVciDriver::isLoaded() const
{
    return m_lib.isLoaded();
}

bool ECanVciDriver::resolveApi(QString *error)
{
    if (m_OpenDevice && m_CloseDevice && m_InitCAN && m_StartCAN && m_Transmit && m_Receive)
    {
        return true;
    }

    m_lib.setFileName(m_libraryPath);
    if (!m_lib.load())
    {
        if (error)
        {
            *error = QStringLiteral("加载%1失败：%2").arg(m_libraryPath, m_lib.errorString());
        }
        return false;
    }

    m_OpenDevice = reinterpret_cast<FnOpenDevice *>(m_lib.resolve("OpenDevice"));
    m_CloseDevice = reinterpret_cast<FnCloseDevice *>(m_lib.resolve("CloseDevice"));
    m_InitCAN = reinterpret_cast<FnInitCAN *>(m_lib.resolve("InitCAN"));
    m_StartCAN = reinterpret_cast<FnStartCAN *>(m_lib.resolve("StartCAN"));
    m_Transmit = reinterpret_cast<FnTransmit *>(m_lib.resolve("Transmit"));
    m_Receive = reinterpret_cast<FnReceive *>(m_lib.resolve("Receive"));
    m_ClearBuffer = reinterpret_cast<FnClearBuffer *>(m_lib.resolve("ClearBuffer"));
    m_ReadErrInfo = reinterpret_cast<FnReadErrInfo *>(m_lib.resolve("ReadErrInfo"));
    m_ReadBoardInfo = reinterpret_cast<FnReadBoardInfo *>(m_lib.resolve("ReadBoardInfo"));

    if (!m_OpenDevice || !m_CloseDevice || !m_InitCAN || !m_StartCAN || !m_Transmit || !m_Receive)
    {
        if (error)
        {
            *error = QStringLiteral("ECanVci DLL缺少必要导出函数。");
        }
        return false;
    }

    return true;
}

bool ECanVciDriver::open(const CanDriverConfig &config, QString *message, QString *error)
{
    if (!resolveApi(error))
    {
        return false;
    }

    close();
    m_config = config;

    if (m_OpenDevice(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex), 0) != STATUS_OK)
    {
        ERR_INFO err = {};
        if (m_ReadErrInfo)
        {
            m_ReadErrInfo(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex), DWORD(m_config.channelIndex), &err);
        }
        if (error)
        {
            *error = QStringLiteral("OpenDevice失败，错误码 0x%1").arg(err.ErrCode, 0, 16).toUpper();
        }
        return false;
    }

    INIT_CONFIG init = {};
    init.AccCode = 0x00000000;
    init.AccMask = 0xFFFFFFFF;
    init.Reserved = 0;
    init.Filter = 0;
    init.Mode = UCHAR(m_config.mode);
    CanBitrate::fillECanVciTiming(m_config.baudIndex, &init.Timing0, &init.Timing1);

    if (m_InitCAN(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex), DWORD(m_config.channelIndex), &init) != STATUS_OK)
    {
        m_CloseDevice(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex));
        if (error)
        {
            *error = QStringLiteral("InitCAN失败。");
        }
        return false;
    }

    if (m_StartCAN(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex), DWORD(m_config.channelIndex)) != STATUS_OK)
    {
        m_CloseDevice(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex));
        if (error)
        {
            *error = QStringLiteral("StartCAN失败。");
        }
        return false;
    }

    m_opened = true;

    if (message)
    {
        BOARD_INFO info = {};
        if (m_ReadBoardInfo && m_ReadBoardInfo(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex), &info) == STATUS_OK)
        {
            *message = formatBoardInfo(info);
        }
        else
        {
            *message = QStringLiteral("设备已连接  类型:%1  设备索引:%2  CAN通道:CAN%3")
                           .arg(driverName())
                           .arg(m_config.deviceIndex)
                           .arg(m_config.channelIndex + 1);
        }
    }

    return true;
}

void ECanVciDriver::close()
{
    if (m_opened && m_CloseDevice)
    {
        m_CloseDevice(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex));
    }
    m_opened = false;
}

bool ECanVciDriver::transmit(const CanDriverFrame &frame, QString *error)
{
    if (!m_opened || !m_Transmit)
    {
        if (error)
        {
            *error = QStringLiteral("CAN设备未打开。");
        }
        return false;
    }
    if (frame.fd)
    {
        if (error)
        {
            *error = QStringLiteral("广成科技 ECanVci 当前只支持普通 CAN 发送。");
        }
        return false;
    }

    CAN_OBJ obj = {};
    obj.ID = frame.id;
    obj.SendType = 0;
    obj.RemoteFlag = frame.rtr ? BYTE(1) : BYTE(0);
    obj.ExternFlag = frame.ext ? BYTE(1) : BYTE(0);
    obj.DataLen = BYTE(qMin(8, frame.data.size()));
    for (int i = 0; i < int(obj.DataLen); ++i)
    {
        obj.Data[i] = BYTE(frame.data.at(i));
    }

    const ULONG sent = m_Transmit(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex), DWORD(m_config.channelIndex), &obj, 1);
    if (sent == 0)
    {
        if (error)
        {
            *error = QStringLiteral("Transmit失败。");
        }
        return false;
    }
    return true;
}

int ECanVciDriver::receive(CanDriverFrame *frames, int maxFrames, int waitMs, QString *error)
{
    if (!m_opened || !m_Receive || frames == nullptr || maxFrames <= 0)
    {
        return 0;
    }

    CAN_OBJ buffer[64];
    const int limit = qMin(maxFrames, 64);
    const ULONG count = m_Receive(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex), DWORD(m_config.channelIndex), buffer, ULONG(limit), waitMs);
    if (count == ULONG(-1) || count == 0xFFFFFFFFu)
    {
        if (error)
        {
            *error = QStringLiteral("Receive失败。");
        }
        return -1;
    }

    const int received = qMin<int>(int(count), limit);
    for (int i = 0; i < received; ++i)
    {
        frames[i].id = buffer[i].ID;
        frames[i].ext = buffer[i].ExternFlag != 0;
        frames[i].rtr = buffer[i].RemoteFlag != 0;
        frames[i].fd = false;
        frames[i].brs = false;
        frames[i].data.clear();
        if (!frames[i].rtr && buffer[i].DataLen > 0)
        {
            frames[i].data.resize(int(buffer[i].DataLen));
            for (int b = 0; b < int(buffer[i].DataLen); ++b)
            {
                frames[i].data[b] = char(buffer[i].Data[b]);
            }
        }
    }
    return received;
}

void ECanVciDriver::clearBuffer()
{
    if (m_opened && m_ClearBuffer)
    {
        m_ClearBuffer(DWORD(m_config.deviceType), DWORD(m_config.deviceIndex), DWORD(m_config.channelIndex));
    }
}

QString ECanVciDriver::formatBoardInfo(const BOARD_INFO &info) const
{
    const QString serial = QString::fromLocal8Bit(info.str_Serial_Num).trimmed();
    const QString hwType = QString::fromLocal8Bit(info.str_hw_Type).trimmed();
    return QStringLiteral("设备已连接  序列号:%1  类型:%2  CAN通道数:%3  当前通道:CAN%4  硬件版本:0x%5  固件版本:0x%6  驱动版本:0x%7  接口版本:0x%8  IRQ:%9")
        .arg(serial.isEmpty() ? QStringLiteral("-") : serial)
        .arg(hwType.isEmpty() ? QStringLiteral("-") : hwType)
        .arg(int(info.can_Num))
        .arg(m_config.channelIndex + 1)
        .arg(info.hw_Version, 0, 16)
        .arg(info.fw_Version, 0, 16)
        .arg(info.dr_Version, 0, 16)
        .arg(info.in_Version, 0, 16)
        .arg(info.irq_Num);
}

ICanDriver *createECanVciDriver()
{
    return new ECanVciDriver;
}
