#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QVector>

struct CanDeviceTypeInfo
{
    CanDeviceTypeInfo() {}
    CanDeviceTypeInfo(const QString &deviceName, int deviceValue)
        : name(deviceName)
        , value(deviceValue)
    {
    }

    QString name;
    int value = 0;
};

struct CanDriverConfig
{
    int deviceType = 0;
    int deviceIndex = 0;
    int channelIndex = 0;
    int baudIndex = 3;
    int mode = 0;
    int frameMode = 0; // 0: classic CAN, 1: CAN FD, 2: CAN FD with BRS
};

struct CanDriverFrame
{
    quint32 id = 0;
    bool ext = false;
    bool rtr = false;
    bool fd = false;
    bool brs = false;
    QByteArray data;
};

class ICanDriver
{
public:
    virtual ~ICanDriver() {}

    virtual QString driverName() const = 0;
    virtual QString libraryPath() const = 0;
    virtual bool isLoaded() const = 0;
    virtual bool open(const CanDriverConfig &config, QString *message, QString *error) = 0;
    virtual void close() = 0;
    virtual bool transmit(const CanDriverFrame &frame, QString *error) = 0;
    virtual int receive(CanDriverFrame *frames, int maxFrames, int waitMs, QString *error) = 0;
    virtual void clearBuffer() = 0;
};

namespace CanDriverFactory
{
QStringList availableDrivers();
QVector<CanDeviceTypeInfo> deviceTypes(const QString &driverName);
ICanDriver *create(const QString &driverName);
}

namespace CanBitrate
{
void fillECanVciTiming(int baudIndex, unsigned char *timing0, unsigned char *timing1);
int bitsPerSecond(int baudIndex);
QString label(int baudIndex);
int count();
}

#endif
