#include "can/can_worker.h"

#include "can/can_driver.h"

#include <QMutexLocker>
#include <QStringList>

namespace
{
QString formatCanId(quint32 id, bool ext)
{
    if (ext)
    {
        return QStringLiteral("%1").arg(id, 8, 16, QLatin1Char('0')).toUpper();
    }
    return QStringLiteral("%1").arg(id & 0x7FFu, 3, 16, QLatin1Char('0')).toUpper();
}

QString toHexSpaced(const QByteArray &data)
{
    QStringList parts;
    for (int i = 0; i < data.size(); ++i)
    {
        parts << QStringLiteral("%1").arg(quint8(data.at(i)), 2, 16, QLatin1Char('0')).toUpper();
    }
    return parts.join(QLatin1Char(' '));
}

QString frameModeText(int frameMode)
{
    switch (frameMode)
    {
    case 1:
        return QStringLiteral("CAN FD");
    case 2:
        return QStringLiteral("CAN FD(加速)");
    default:
        return QStringLiteral("普通CAN");
    }
}
}

CanWorker::CanWorker(QObject *parent)
    : QThread(parent)
{
}

CanWorker::~CanWorker()
{
    closeDevice();
}

bool CanWorker::openDevice(const QString &driverName, const CanDriverConfig &config)
{
    closeDevice();

    QString error;
    m_driver.reset(CanDriverFactory::create(driverName));
    if (!m_driver)
    {
        emit openFailed(QStringLiteral("不支持的CAN驱动：%1").arg(driverName));
        return false;
    }

    QString message;
    if (!m_driver->open(config, &message, &error))
    {
        emit openFailed(error.isEmpty() ? QStringLiteral("打开CAN设备失败。") : error);
        m_driver.reset();
        return false;
    }

    m_config = config;
    m_opened = true;
    m_recvRunning = true;
    m_stopped = false;
    m_timer.start();
    message += QStringLiteral("  协议:%1").arg(frameModeText(m_config.frameMode));
    emit deviceOpened(message);
    start();
    return true;
}

void CanWorker::closeDevice()
{
    if (isRunning())
    {
        stopWorker();
        wait(1500);
    }

    if (m_driver)
    {
        m_driver->close();
    }

    if (m_opened)
    {
        m_opened = false;
        emit deviceClosed();
    }
}

void CanWorker::stopWorker()
{
    m_recvRunning = false;
    m_stopped = true;
}

void CanWorker::transmitFrame(quint32 id, const QByteArray &data, bool ext, bool rtr)
{
    QMutexLocker locker(&m_txMutex);
    if (!m_opened || !m_driver)
    {
        return;
    }

    CanDriverFrame frame;
    frame.id = id;
    frame.ext = ext;
    frame.rtr = rtr;
    frame.fd = m_config.frameMode != 0;
    frame.brs = m_config.frameMode == 2;
    frame.data = frame.fd ? data.left(64) : data.left(8);

    QString error;
    if (m_driver->transmit(frame, &error))
    {
        emitFrame(QStringLiteral("TX"), frame);
    }
    else if (!error.isEmpty())
    {
        emit openFailed(error);
    }
}

void CanWorker::clearRxBuffer()
{
    if (m_driver)
    {
        m_driver->clearBuffer();
    }
}

void CanWorker::setReceiveFilter(bool enabled, const QVector<quint32> &ids, bool ext)
{
    QMutexLocker locker(&m_filterMutex);
    m_filterEnabled = enabled;
    m_filterIds = ids;
    m_filterExt = ext;
}

void CanWorker::emitFrame(const QString &dir, const CanDriverFrame &frame)
{
    emit frameReceived(m_timer.isValid() ? QString::number(m_timer.elapsed()) : QStringLiteral("0"),
                       dir,
                       formatCanId(frame.id, frame.ext),
                       frameTypeString(frame.ext, frame.rtr, frame.fd, frame.brs),
                       QString::number(frame.data.size()),
                       frame.rtr ? QString() : toHexSpaced(frame.data),
                       frame.id,
                       frame.ext,
                       frame.rtr,
                       frame.rtr ? QByteArray() : frame.data);
}

bool CanWorker::shouldEmitReceivedFrame(const CanDriverFrame &frame)
{
    QMutexLocker locker(&m_filterMutex);
    if (!m_filterEnabled)
    {
        return true;
    }
    return frame.ext == m_filterExt && m_filterIds.contains(frame.id);
}

QString CanWorker::frameTypeString(bool ext, bool rtr, bool fd, bool brs) const
{
    if (fd)
    {
        return QStringLiteral("CAN FD%1%2帧")
            .arg(brs ? QStringLiteral("(加速)") : QString())
            .arg(ext ? QStringLiteral("扩展") : QStringLiteral("标准"));
    }

    return QStringLiteral("%1%2帧")
        .arg(ext ? QStringLiteral("扩展") : QStringLiteral("标准"))
        .arg(rtr ? QStringLiteral("远程") : QStringLiteral("数据"));
}

void CanWorker::run()
{
    CanDriverFrame frames[64];

    while (!m_stopped)
    {
        if (!m_opened || !m_recvRunning || !m_driver)
        {
            msleep(5);
            continue;
        }

        QString error;
        const int count = m_driver->receive(frames, 64, 10, &error);
        if (count < 0)
        {
            msleep(10);
            continue;
        }

        for (int i = 0; i < count; ++i)
        {
            if (shouldEmitReceivedFrame(frames[i]))
            {
                emitFrame(QStringLiteral("RX"), frames[i]);
            }
        }
        msleep(1);
    }
}
