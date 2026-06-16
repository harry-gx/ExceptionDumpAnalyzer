#include "can/can_driver.h"

ICanDriver *createECanVciDriver();
ICanDriver *createZlgCanDriver();

namespace
{
const char *const kECanVciDriverName = "广成科技 ECanVci";
const char *const kZlgCanDriverName = "周立功 USBCANFD";
}

namespace CanDriverFactory
{
QStringList availableDrivers()
{
    return QStringList()
        << QString::fromUtf8(kECanVciDriverName)
        << QString::fromUtf8(kZlgCanDriverName);
}

QVector<CanDeviceTypeInfo> deviceTypes(const QString &driverName)
{
    QVector<CanDeviceTypeInfo> types;

    if (driverName == QString::fromUtf8(kZlgCanDriverName))
    {
        types.append({QStringLiteral("USBCANFD-200U"), 41});
        types.append({QStringLiteral("USBCANFD-MINI"), 43});
        return types;
    }

    types.append({QStringLiteral("USBCAN-I"), 3});
    return types;
}

ICanDriver *create(const QString &driverName)
{
    if (driverName == QString::fromUtf8(kECanVciDriverName))
    {
        return createECanVciDriver();
    }
    if (driverName == QString::fromUtf8(kZlgCanDriverName))
    {
        return createZlgCanDriver();
    }
    return nullptr;
}
}

namespace CanBitrate
{
void fillECanVciTiming(int baudIndex, unsigned char *timing0, unsigned char *timing1)
{
    if (!timing0 || !timing1)
    {
        return;
    }

    switch (baudIndex)
    {
    case 0: *timing0 = 0x00; *timing1 = 0x14; break;
    case 1: *timing0 = 0x00; *timing1 = 0x16; break;
    case 2: *timing0 = 0x80; *timing1 = 0xB6; break;
    case 3: *timing0 = 0x00; *timing1 = 0x1C; break;
    case 4: *timing0 = 0x80; *timing1 = 0xFA; break;
    case 5: *timing0 = 0x01; *timing1 = 0x1C; break;
    case 6: *timing0 = 0x81; *timing1 = 0xFA; break;
    case 7: *timing0 = 0x03; *timing1 = 0x1C; break;
    case 8: *timing0 = 0x04; *timing1 = 0x1C; break;
    case 9: *timing0 = 0x83; *timing1 = 0xFF; break;
    case 10: *timing0 = 0x09; *timing1 = 0x1C; break;
    default: *timing0 = 0x00; *timing1 = 0x1C; break;
    }
}

int bitsPerSecond(int baudIndex)
{
    static const int kValues[] = {
        1000000, 800000, 666000, 500000, 400000, 250000, 200000, 125000, 100000, 80000, 50000,
    };
    const int n = int(sizeof(kValues) / sizeof(kValues[0]));
    if (baudIndex >= 0 && baudIndex < n)
    {
        return kValues[baudIndex];
    }
    return 500000;
}

QString label(int baudIndex)
{
    static const char *const kNames[] = {
        "1000k", "800k", "666k", "500k", "400k", "250k", "200k", "125k", "100k", "80k", "50k",
    };
    const int n = int(sizeof(kNames) / sizeof(kNames[0]));
    if (baudIndex >= 0 && baudIndex < n)
    {
        return QString::fromLatin1(kNames[baudIndex]);
    }
    return QStringLiteral("?");
}

int count()
{
    return 11;
}
}
