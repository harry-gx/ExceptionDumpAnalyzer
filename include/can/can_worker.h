#ifndef CAN_WORKER_H
#define CAN_WORKER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QMutex>
#include <QScopedPointer>
#include <QThread>
#include <QVector>

#include "can/can_driver.h"

class CanWorker : public QThread
{
    Q_OBJECT

public:
    explicit CanWorker(QObject *parent = nullptr);
    ~CanWorker() override;

    bool openDevice(const QString &driverName, const CanDriverConfig &config);
    void closeDevice();
    void stopWorker();
    void transmitFrame(quint32 id, const QByteArray &data, bool ext, bool rtr);
    void clearRxBuffer();
    void setReceiveFilter(bool enabled, const QVector<quint32> &ids, bool ext);

    bool isDeviceOpen() const { return m_opened; }
    int frameMode() const { return m_config.frameMode; }

signals:
    void frameReceived(const QString &timeMs, const QString &dir, const QString &idHex,
                       const QString &frameType, const QString &dlc, const QString &dataHex,
                       quint32 id, bool ext, bool rtr, const QByteArray &rawData);
    void deviceOpened(const QString &message);
    void openFailed(const QString &reason);
    void deviceClosed();

protected:
    void run() override;

private:
    QString frameTypeString(bool ext, bool rtr, bool fd, bool brs) const;
    void emitFrame(const QString &dir, const CanDriverFrame &frame);
    bool shouldEmitReceivedFrame(const CanDriverFrame &frame);

    QScopedPointer<ICanDriver> m_driver;
    QMutex m_txMutex;
    QMutex m_filterMutex;
    volatile bool m_stopped = false;
    volatile bool m_recvRunning = false;
    bool m_opened = false;
    bool m_filterEnabled = false;
    QVector<quint32> m_filterIds;
    bool m_filterExt = false;
    CanDriverConfig m_config;
    QElapsedTimer m_timer;
};

#endif
