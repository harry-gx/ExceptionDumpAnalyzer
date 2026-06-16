#ifndef EXCEPTION_RECORD_H
#define EXCEPTION_RECORD_H

#include <QtCore>

struct RegisterRow
{
    QString group;
    QString name;
    quint32 value;
    QString description;
};

struct ExceptionRecord
{
    QString formatName;
    qint64 offset = 0;
    int slotIndex = 0;
    int slotSize = 0;
    bool complete = false;
    bool crcAvailable = false;
    bool crcOk = false;

    quint32 magic = 0;
    QVector<quint32> versionWords;
    quint32 recordIndex = 0;
    quint32 recordState = 0;
    quint32 storedCrc = 0;
    quint32 computedCrc = 0;

    quint32 r0 = 0;
    quint32 r1 = 0;
    quint32 r2 = 0;
    quint32 r3 = 0;
    quint32 r12 = 0;
    quint32 lr = 0;
    quint32 pc = 0;
    quint32 xpsr = 0;

    quint32 r4 = 0;
    quint32 r5 = 0;
    quint32 r6 = 0;
    quint32 r7 = 0;
    quint32 r8 = 0;
    quint32 r9 = 0;
    quint32 r10 = 0;
    quint32 r11 = 0;

    quint32 msp = 0;
    quint32 psp = 0;
    quint32 excReturn = 0;
    quint32 control = 0;
    quint32 primask = 0;
    quint32 basepri = 0;
    quint32 faultmask = 0;
    quint32 faultType = 0;

    quint32 cfsr = 0;
    quint32 hfsr = 0;
    quint32 dfsr = 0;
    quint32 afsr = 0;
    quint32 mmfar = 0;
    quint32 bfar = 0;
    quint32 shcsr = 0;
    quint32 ccr = 0;

    quint32 stackStartAddr = 0;
    quint32 stackSize = 0;
    quint32 stackLimit = 0;
    quint32 stackTop = 0;
    QByteArray stackData;

    QStringList warnings;

    QVector<RegisterRow> registerRows() const;
    QVector<quint32> stackWords() const;
};

struct ParseResult
{
    QVector<ExceptionRecord> records;
    QString chosenFormat;
    QStringList messages;
};

class ExceptionDumpParser
{
public:
    ParseResult parseFile(const QString &path, QString *errorMessage) const;

    static QString hex32(quint32 value);
    static QString stateName(quint32 state);
    static QString faultTypeName(quint32 faultType);
    static QString excReturnDescription(quint32 excReturn);
    static QStringList decodeFaultBits(const ExceptionRecord &record);
    static QStringList stackSanityWarnings(const ExceptionRecord &record);

private:
    static quint32 readU32(const QByteArray &data, qint64 offset);
    static quint32 crc32(const uchar *data, int length);
    static ExceptionRecord parseOne(const QByteArray &data, qint64 offset, int slotIndex);
    static QVector<ExceptionRecord> parseRecords(const QByteArray &data, bool scanForMagic);
};

#endif
