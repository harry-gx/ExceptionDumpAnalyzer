#include "can/zlg_can_driver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <cstring>

namespace
{
QString resolveZlgCanLibrary()
{
#ifdef Q_OS_WIN
    const QString dllName = QStringLiteral("zlgcan.dll");
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;

    candidates << QDir(appDir).absoluteFilePath(dllName)
               << QDir(appDir).absoluteFilePath(QStringLiteral("lib/zlg/") + dllName)
               << QDir::current().absoluteFilePath(QStringLiteral("lib/zlg/") + dllName);

    QDir walk(appDir);
    for (int depth = 0; depth < 12; ++depth)
    {
        candidates << walk.absoluteFilePath(QStringLiteral("lib/zlg/") + dllName);
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
    return QStringLiteral("zlgcan");
}

canid_t makeZlgCanId(const CanDriverFrame &frame)
{
    return MAKE_CAN_ID(frame.id, frame.ext ? 1 : 0, frame.rtr ? 1 : 0, 0);
}

QString zlgVersionText(const ZCAN_VERSION &version)
{
    return QStringLiteral("%1.%2.%3")
        .arg(int(version.major_version))
        .arg(int(version.minor_version))
        .arg(int(version.patch_version));
}
}

ZlgCanDriver::ZlgCanDriver()
    : m_libraryPath(resolveZlgCanLibrary())
{
}

ZlgCanDriver::~ZlgCanDriver()
{
    close();
}

QString ZlgCanDriver::driverName() const
{
    return QStringLiteral("周立功 USBCANFD");
}

QString ZlgCanDriver::libraryPath() const
{
    return m_libraryPath;
}

bool ZlgCanDriver::isLoaded() const
{
    return m_lib.isLoaded();
}

bool ZlgCanDriver::resolveApi(QString *error)
{
    if (m_OpenDevice && m_CloseDevice && m_InitCAN && m_StartCAN && m_ClearBuffer && m_GetReceiveNum && m_Transmit && m_TransmitFD && m_Receive && m_ReceiveFD && m_SetValue)
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

    m_OpenDevice = reinterpret_cast<FnOpenDevice>(m_lib.resolve("ZCAN_OpenDevice"));
    m_CloseDevice = reinterpret_cast<FnCloseDevice>(m_lib.resolve("ZCAN_CloseDevice"));
    m_GetDeviceInfoEx = reinterpret_cast<FnGetDeviceInfoEx>(m_lib.resolve("ZCAN_GetDeviceInfoEx"));
    m_InitCAN = reinterpret_cast<FnInitCAN>(m_lib.resolve("ZCAN_InitCAN"));
    m_StartCAN = reinterpret_cast<FnStartCAN>(m_lib.resolve("ZCAN_StartCAN"));
    m_ResetCAN = reinterpret_cast<FnResetCAN>(m_lib.resolve("ZCAN_ResetCAN"));
    m_ClearBuffer = reinterpret_cast<FnClearBuffer>(m_lib.resolve("ZCAN_ClearBuffer"));
    m_GetReceiveNum = reinterpret_cast<FnGetReceiveNum>(m_lib.resolve("ZCAN_GetReceiveNum"));
    m_Transmit = reinterpret_cast<FnTransmit>(m_lib.resolve("ZCAN_Transmit"));
    m_TransmitFD = reinterpret_cast<FnTransmitFD>(m_lib.resolve("ZCAN_TransmitFD"));
    m_Receive = reinterpret_cast<FnReceive>(m_lib.resolve("ZCAN_Receive"));
    m_ReceiveFD = reinterpret_cast<FnReceiveFD>(m_lib.resolve("ZCAN_ReceiveFD"));
    m_SetValue = reinterpret_cast<FnSetValue>(m_lib.resolve("ZCAN_SetValue"));

    if (!m_OpenDevice || !m_CloseDevice || !m_InitCAN || !m_StartCAN || !m_ClearBuffer || !m_GetReceiveNum || !m_Transmit || !m_TransmitFD || !m_Receive || !m_ReceiveFD || !m_SetValue)
    {
        if (error)
        {
            *error = QStringLiteral("zlgcan.dll缺少必要导出函数。");
        }
        return false;
    }

    return true;
}

bool ZlgCanDriver::setChannelValue(const QString &key, const QByteArray &value, QString *error)
{
    if (!m_SetValue || m_device == INVALID_DEVICE_HANDLE)
    {
        return false;
    }

    const QByteArray path = QStringLiteral("%1/%2").arg(m_config.channelIndex).arg(key).toLatin1();
    if (m_SetValue(m_device, path.constData(), value.constData()) == STATUS_ERR)
    {
        if (error)
        {
            *error = QStringLiteral("设置周立功通道参数失败：%1=%2").arg(QString::fromLatin1(path), QString::fromLatin1(value));
        }
        return false;
    }
    return true;
}

bool ZlgCanDriver::open(const CanDriverConfig &config, QString *message, QString *error)
{
    if (!resolveApi(error))
    {
        return false;
    }

    close();
    m_config = config;

    if (m_config.deviceType != ZCAN_USBCANFD_200U && m_config.deviceType != ZCAN_USBCANFD_MINI)
    {
        if (error)
        {
            *error = QStringLiteral("当前只适配USBCANFD-200U和USBCANFD-MINI。");
        }
        return false;
    }

    m_device = m_OpenDevice(UINT(m_config.deviceType), UINT(m_config.deviceIndex), 0);
    if (m_device == INVALID_DEVICE_HANDLE)
    {
        if (error)
        {
            *error = QStringLiteral("ZCAN_OpenDevice失败，设备类型=%1，索引=%2。").arg(m_config.deviceType).arg(m_config.deviceIndex);
        }
        return false;
    }

    if (!setChannelValue(QStringLiteral("canfd_abit_baud_rate"), QByteArray::number(CanBitrate::bitsPerSecond(m_config.baudIndex)), error) ||
        !setChannelValue(QStringLiteral("canfd_dbit_baud_rate"), QByteArrayLiteral("2000000"), error))
    {
        close();
        return false;
    }

    ZCAN_CHANNEL_INIT_CONFIG init;
    std::memset(&init, 0, sizeof(init));
    init.can_type = TYPE_CANFD;
    init.canfd.mode = BYTE(m_config.mode);

    m_channel = m_InitCAN(m_device, UINT(m_config.channelIndex), &init);
    if (m_channel == INVALID_CHANNEL_HANDLE)
    {
        if (error)
        {
            *error = QStringLiteral("ZCAN_InitCAN失败，通道=%1。").arg(m_config.channelIndex);
        }
        close();
        return false;
    }

    setChannelValue(QStringLiteral("initenal_resistance"), QByteArrayLiteral("1"), nullptr);

    if (m_StartCAN(m_channel) == STATUS_ERR)
    {
        if (error)
        {
            *error = QStringLiteral("ZCAN_StartCAN失败。");
        }
        close();
        return false;
    }

    // The official USBCANFD demo disables merged receive before using
    // channel-level ZCAN_Receive. If the device remains in merged receive
    // mode, TX can still work while this receive path sees no frames.
    m_SetValue(m_device, "0/set_device_recv_merge", "0");
    if (m_config.channelIndex != 0)
    {
        const QByteArray mergePath = QStringLiteral("%1/set_device_recv_merge").arg(m_config.channelIndex).toLatin1();
        m_SetValue(m_device, mergePath.constData(), "0");
    }
    m_ClearBuffer(m_channel);

    m_opened = true;

    if (message)
    {
        ZCAN_DEVICE_INFO_EX info;
        std::memset(&info, 0, sizeof(info));
        if (m_GetDeviceInfoEx && m_GetDeviceInfoEx(m_device, &info) == STATUS_OK)
        {
            *message = formatDeviceInfo(info);
        }
        else
        {
            *message = QStringLiteral("设备已连接  设备:%1  设备索引:%2  当前通道:CAN%3")
                           .arg(m_config.deviceType == ZCAN_USBCANFD_200U ? QStringLiteral("USBCANFD-200U") : QStringLiteral("USBCANFD-MINI"))
                           .arg(m_config.deviceIndex)
                           .arg(m_config.channelIndex + 1)
                           ;
        }
    }

    return true;
}

void ZlgCanDriver::close()
{
    if (m_channel != INVALID_CHANNEL_HANDLE && m_ResetCAN)
    {
        m_ResetCAN(m_channel);
    }
    m_channel = INVALID_CHANNEL_HANDLE;

    if (m_device != INVALID_DEVICE_HANDLE && m_CloseDevice)
    {
        m_CloseDevice(m_device);
    }
    m_device = INVALID_DEVICE_HANDLE;
    m_opened = false;
}

bool ZlgCanDriver::transmit(const CanDriverFrame &frame, QString *error)
{
    if (!m_opened || !m_Transmit || !m_TransmitFD || m_channel == INVALID_CHANNEL_HANDLE)
    {
        if (error)
        {
            *error = QStringLiteral("周立功CAN设备未打开。");
        }
        return false;
    }

    if (frame.fd)
    {
        ZCAN_TransmitFD_Data tx;
        std::memset(&tx, 0, sizeof(tx));
        tx.frame.can_id = makeZlgCanId(frame);
        tx.frame.len = BYTE(qMin(CANFD_MAX_DLEN, frame.data.size()));
        tx.frame.flags = frame.brs ? CANFD_BRS : 0;
        for (int i = 0; i < int(tx.frame.len); ++i)
        {
            tx.frame.data[i] = BYTE(frame.data.at(i));
        }
        tx.transmit_type = 0;

        const UINT sent = m_TransmitFD(m_channel, &tx, 1);
        if (sent == 0)
        {
            if (error)
            {
                *error = QStringLiteral("ZCAN_TransmitFD失败。");
            }
            return false;
        }
        return true;
    }

    ZCAN_Transmit_Data tx;
    std::memset(&tx, 0, sizeof(tx));
    tx.frame.can_id = makeZlgCanId(frame);
    tx.frame.can_dlc = BYTE(qMin(8, frame.data.size()));
    for (int i = 0; i < int(tx.frame.can_dlc); ++i)
    {
        tx.frame.data[i] = BYTE(frame.data.at(i));
    }
    tx.transmit_type = 0;

    const UINT sent = m_Transmit(m_channel, &tx, 1);
    if (sent == 0)
    {
        if (error)
        {
            *error = QStringLiteral("ZCAN_Transmit失败。");
        }
        return false;
    }
    return true;
}

int ZlgCanDriver::receive(CanDriverFrame *frames, int maxFrames, int waitMs, QString *error)
{
    if (!m_opened || !m_GetReceiveNum || !m_Receive || !m_ReceiveFD || m_channel == INVALID_CHANNEL_HANDLE || frames == nullptr || maxFrames <= 0)
    {
        return 0;
    }

    if (m_config.frameMode != 0)
    {
        const UINT fdAvailable = m_GetReceiveNum(m_channel, TYPE_CANFD);
        if (fdAvailable == 0)
        {
            return 0;
        }

        ZCAN_ReceiveFD_Data fdBuffer[64];
        std::memset(fdBuffer, 0, sizeof(fdBuffer));
        const int fdLimit = qMin<int>(qMin(maxFrames, 64), int(fdAvailable));
        const UINT fdCount = m_ReceiveFD(m_channel, fdBuffer, UINT(fdLimit), waitMs);
        if (fdCount == UINT(-1) || fdCount == 0xFFFFFFFFu)
        {
            if (error)
            {
                *error = QStringLiteral("ZCAN_ReceiveFD失败。");
            }
            return -1;
        }

        const int fdReceived = qMin<int>(int(fdCount), fdLimit);
        for (int i = 0; i < fdReceived; ++i)
        {
            const canid_t canId = fdBuffer[i].frame.can_id;
            frames[i].id = GET_ID(canId);
            frames[i].ext = IS_EFF(canId) != 0;
            frames[i].rtr = false;
            frames[i].fd = true;
            frames[i].brs = (fdBuffer[i].frame.flags & CANFD_BRS) != 0;
            frames[i].data.clear();

            const int len = qMin<int>(int(fdBuffer[i].frame.len), CANFD_MAX_DLEN);
            if (len > 0)
            {
                frames[i].data.resize(len);
                for (int b = 0; b < len; ++b)
                {
                    frames[i].data[b] = char(fdBuffer[i].frame.data[b]);
                }
            }
        }
        return fdReceived;
    }

    ZCAN_Receive_Data buffer[64];
    std::memset(buffer, 0, sizeof(buffer));
    const UINT available = m_GetReceiveNum(m_channel, TYPE_CAN);
    if (available == 0)
    {
        return 0;
    }

    const int limit = qMin<int>(qMin(maxFrames, 64), int(available));
    const UINT count = m_Receive(m_channel, buffer, UINT(limit), waitMs);
    if (count == UINT(-1) || count == 0xFFFFFFFFu)
    {
        if (error)
        {
            *error = QStringLiteral("ZCAN_Receive澶辫触銆?");
        }
        return -1;
    }
    const int received = qMin<int>(int(count), limit);

    for (int i = 0; i < received; ++i)
    {
        const canid_t canId = buffer[i].frame.can_id;
        frames[i].id = GET_ID(canId);
        frames[i].ext = IS_EFF(canId) != 0;
        frames[i].rtr = IS_RTR(canId) != 0;
        frames[i].fd = false;
        frames[i].brs = false;
        frames[i].data.clear();

        const int len = qMin<int>(int(buffer[i].frame.can_dlc), CAN_MAX_DLEN);
        if (!frames[i].rtr && len > 0)
        {
            frames[i].data.resize(len);
            for (int b = 0; b < len; ++b)
            {
                frames[i].data[b] = char(buffer[i].frame.data[b]);
            }
        }
    }

    return received;
}

void ZlgCanDriver::clearBuffer()
{
    if (m_opened && m_ClearBuffer && m_channel != INVALID_CHANNEL_HANDLE)
    {
        m_ClearBuffer(m_channel);
    }
}

QString ZlgCanDriver::formatDeviceInfo(const ZCAN_DEVICE_INFO_EX &info) const
{
    const QString deviceName = QString::fromLocal8Bit(reinterpret_cast<const char *>(info.device_name)).trimmed();
    const QString hardwareType = QString::fromLocal8Bit(reinterpret_cast<const char *>(info.hardware_type)).trimmed();
    const QString serial = QString::fromLocal8Bit(reinterpret_cast<const char *>(info.serial_number)).trimmed();

    return QStringLiteral("设备已连接  名称:%1  类型:%2  序列号:%3  CAN通道数:%4  LIN通道数:%5  当前通道:CAN%6  硬件版本:%7  固件版本:%8  驱动版本:%9  动态库版本:%10  设备信息版本:%11")
        .arg(deviceName.isEmpty() ? QStringLiteral("-") : deviceName)
        .arg(hardwareType.isEmpty() ? QStringLiteral("-") : hardwareType)
        .arg(serial.isEmpty() ? QStringLiteral("-") : serial)
        .arg(int(info.can_channel_number))
        .arg(int(info.lin_channel_number))
        .arg(m_config.channelIndex + 1)
        .arg(zlgVersionText(info.hardware_version))
        .arg(zlgVersionText(info.firmware_version))
        .arg(zlgVersionText(info.driver_version))
        .arg(zlgVersionText(info.library_version))
        .arg(zlgVersionText(info.device_info_version));
}

ICanDriver *createZlgCanDriver()
{
    return new ZlgCanDriver;
}
