#include "core/exception_record.h"

#include <algorithm>

namespace
{
const quint32 kMagic = 0x45584350u;
const quint32 kStateEmpty = 0xFFFFFFFFu;
const quint32 kStateValid = 0xFFFFFFFCu;
const quint32 kStateInvalid = 0x00000000u;
const int kHeaderSize = 32;
const int kSlotSize = 8192;
const int kRecordIndexOffset = 20;
const int kRecordStateOffset = 24;
const int kCrcOffset = 8188;
const int kHwFrameOffset = 32;
const int kSwRegsOffset = 64;
const int kSpecialRegsOffset = 96;
const int kFaultRegsOffset = 128;
const int kStackInfoOffset = 160;
const int kStackDataOffset = 192;
const int kStackSnapshotSize = 4096;

QString bitLine(const QString &reg, const QString &bit, bool active, const QString &desc)
{
    return QStringLiteral("[%1] %2 %3 - %4")
        .arg(reg, bit, active ? QStringLiteral("置位") : QStringLiteral("未置位"), desc);
}

void appendFault(QStringList *out, const QString &reg, quint32 value, int bit, const QString &name, const QString &desc)
{
    if ((value & (1u << bit)) != 0u)
    {
        out->append(bitLine(reg, name, true, desc));
    }
}
}

QString ExceptionDumpParser::hex32(quint32 value)
{
    return QStringLiteral("0x%1").arg(value, 8, 16, QLatin1Char('0')).toUpper();
}

QString ExceptionDumpParser::stateName(quint32 state)
{
    if (state == kStateEmpty)
    {
        return QStringLiteral("EMPTY");
    }
    if (state == kStateValid)
    {
        return QStringLiteral("VALID");
    }
    if (state == kStateInvalid)
    {
        return QStringLiteral("INVALID");
    }
    return QStringLiteral("UNKNOWN");
}

QString ExceptionDumpParser::faultTypeName(quint32 faultType)
{
    switch (faultType)
    {
    case 1u:
        return QStringLiteral("NMI");
    case 2u:
        return QStringLiteral("HardFault");
    case 3u:
        return QStringLiteral("MemManage");
    case 4u:
        return QStringLiteral("BusFault");
    case 5u:
        return QStringLiteral("UsageFault");
    default:
        return QStringLiteral("Unknown");
    }
}

QString ExceptionDumpParser::excReturnDescription(quint32 excReturn)
{
    QStringList parts;

    if ((excReturn & 0xFF000000u) == 0xFF000000u)
    {
        parts << QStringLiteral("EXC_RETURN");
    }
    parts << (((excReturn & (1u << 2)) != 0u) ? QStringLiteral("返回使用 PSP") : QStringLiteral("返回使用 MSP"));
    parts << (((excReturn & (1u << 3)) != 0u) ? QStringLiteral("返回 Thread 模式") : QStringLiteral("返回 Handler 模式"));
    parts << (((excReturn & (1u << 4)) != 0u) ? QStringLiteral("无浮点扩展栈帧") : QStringLiteral("含浮点扩展栈帧"));

    return parts.join(QStringLiteral("，"));
}

QStringList ExceptionDumpParser::decodeFaultBits(const ExceptionRecord &record)
{
    QStringList out;

    appendFault(&out, QStringLiteral("CFSR/MMFSR"), record.cfsr, 0, QStringLiteral("IACCVIOL"), QStringLiteral("指令访问违规"));
    appendFault(&out, QStringLiteral("CFSR/MMFSR"), record.cfsr, 1, QStringLiteral("DACCVIOL"), QStringLiteral("数据访问违规"));
    appendFault(&out, QStringLiteral("CFSR/MMFSR"), record.cfsr, 3, QStringLiteral("MUNSTKERR"), QStringLiteral("异常返回出栈时发生 MemManage fault"));
    appendFault(&out, QStringLiteral("CFSR/MMFSR"), record.cfsr, 4, QStringLiteral("MSTKERR"), QStringLiteral("异常进入压栈时发生 MemManage fault"));
    appendFault(&out, QStringLiteral("CFSR/MMFSR"), record.cfsr, 5, QStringLiteral("MLSPERR"), QStringLiteral("浮点懒保存期间发生 MemManage fault"));
    appendFault(&out, QStringLiteral("CFSR/MMFSR"), record.cfsr, 7, QStringLiteral("MMARVALID"), QStringLiteral("MMFAR 中保存了有效故障地址"));

    appendFault(&out, QStringLiteral("CFSR/BFSR"), record.cfsr, 8, QStringLiteral("IBUSERR"), QStringLiteral("取指总线错误"));
    appendFault(&out, QStringLiteral("CFSR/BFSR"), record.cfsr, 9, QStringLiteral("PRECISERR"), QStringLiteral("精确数据总线错误，PC 通常可信"));
    appendFault(&out, QStringLiteral("CFSR/BFSR"), record.cfsr, 10, QStringLiteral("IMPRECISERR"), QStringLiteral("非精确数据总线错误，PC 可能滞后"));
    appendFault(&out, QStringLiteral("CFSR/BFSR"), record.cfsr, 11, QStringLiteral("UNSTKERR"), QStringLiteral("异常返回出栈时发生 BusFault"));
    appendFault(&out, QStringLiteral("CFSR/BFSR"), record.cfsr, 12, QStringLiteral("STKERR"), QStringLiteral("异常进入压栈时发生 BusFault"));
    appendFault(&out, QStringLiteral("CFSR/BFSR"), record.cfsr, 13, QStringLiteral("LSPERR"), QStringLiteral("浮点懒保存期间发生 BusFault"));
    appendFault(&out, QStringLiteral("CFSR/BFSR"), record.cfsr, 15, QStringLiteral("BFARVALID"), QStringLiteral("BFAR 中保存了有效故障地址"));

    appendFault(&out, QStringLiteral("CFSR/UFSR"), record.cfsr, 16, QStringLiteral("UNDEFINSTR"), QStringLiteral("未定义指令"));
    appendFault(&out, QStringLiteral("CFSR/UFSR"), record.cfsr, 17, QStringLiteral("INVSTATE"), QStringLiteral("非法 EPSR/T 位状态"));
    appendFault(&out, QStringLiteral("CFSR/UFSR"), record.cfsr, 18, QStringLiteral("INVPC"), QStringLiteral("非法 EXC_RETURN 或 PC"));
    appendFault(&out, QStringLiteral("CFSR/UFSR"), record.cfsr, 19, QStringLiteral("NOCP"), QStringLiteral("访问未使能协处理器"));
    appendFault(&out, QStringLiteral("CFSR/UFSR"), record.cfsr, 24, QStringLiteral("UNALIGNED"), QStringLiteral("非对齐访问"));
    appendFault(&out, QStringLiteral("CFSR/UFSR"), record.cfsr, 25, QStringLiteral("DIVBYZERO"), QStringLiteral("除 0"));

    appendFault(&out, QStringLiteral("HFSR"), record.hfsr, 1, QStringLiteral("VECTTBL"), QStringLiteral("向量表读取失败"));
    appendFault(&out, QStringLiteral("HFSR"), record.hfsr, 30, QStringLiteral("FORCED"), QStringLiteral("可配置 fault 被升级为 HardFault"));
    appendFault(&out, QStringLiteral("HFSR"), record.hfsr, 31, QStringLiteral("DEBUGEVT"), QStringLiteral("调试事件触发 HardFault"));

    appendFault(&out, QStringLiteral("DFSR"), record.dfsr, 0, QStringLiteral("HALTED"), QStringLiteral("调试 halt 请求"));
    appendFault(&out, QStringLiteral("DFSR"), record.dfsr, 1, QStringLiteral("BKPT"), QStringLiteral("断点事件"));
    appendFault(&out, QStringLiteral("DFSR"), record.dfsr, 2, QStringLiteral("DWTTRAP"), QStringLiteral("DWT 触发"));
    appendFault(&out, QStringLiteral("DFSR"), record.dfsr, 3, QStringLiteral("VCATCH"), QStringLiteral("向量捕获"));
    appendFault(&out, QStringLiteral("DFSR"), record.dfsr, 4, QStringLiteral("EXTERNAL"), QStringLiteral("外部调试请求"));

    if ((record.cfsr & (1u << 7)) != 0u)
    {
        out << QStringLiteral("MMFAR 有效：%1").arg(hex32(record.mmfar));
    }
    if ((record.cfsr & (1u << 15)) != 0u)
    {
        out << QStringLiteral("BFAR 有效：%1").arg(hex32(record.bfar));
    }

    if (out.isEmpty())
    {
        out << QStringLiteral("CFSR/HFSR/DFSR 未发现已知 fault 标志位。");
    }

    return out;
}

QStringList ExceptionDumpParser::stackSanityWarnings(const ExceptionRecord &record)
{
    QStringList warnings;

    if (record.stackSize > kStackSnapshotSize)
    {
        warnings << QStringLiteral("stackSize 大于 4096，栈快照长度异常。");
    }

    if (record.stackLimit != 0u || record.stackTop != 0u)
    {
        if (record.stackLimit >= record.stackTop)
        {
            warnings << QStringLiteral("stackLimit >= stackTop，栈边界异常。");
        }
        if (record.stackStartAddr < record.stackLimit || record.stackStartAddr > record.stackTop)
        {
            warnings << QStringLiteral("stackStartAddr 不在 [stackLimit, stackTop] 范围内。");
        }
        const quint64 end = static_cast<quint64>(record.stackStartAddr) + static_cast<quint64>(record.stackSize);
        if (end > record.stackTop)
        {
            warnings << QStringLiteral("stackStartAddr + stackSize 超出 stackTop。");
        }
    }

    if ((record.xpsr & (1u << 24)) == 0u)
    {
        warnings << QStringLiteral("xPSR.T 位未置位，可能不是合法 Thumb 执行现场。");
    }

    return warnings;
}

QVector<RegisterRow> ExceptionRecord::registerRows() const
{
    QVector<RegisterRow> rows;
    const auto add = [&rows](const QString &group, const QString &name, quint32 value, const QString &desc) {
        RegisterRow row;
        row.group = group;
        row.name = name;
        row.value = value;
        row.description = desc;
        rows.append(row);
    };

    add(QStringLiteral("硬件栈帧"), QStringLiteral("R0"), r0, QStringLiteral("函数参数 / 临时变量 / 返回值"));
    add(QStringLiteral("硬件栈帧"), QStringLiteral("R1"), r1, QStringLiteral("函数参数 / 临时变量"));
    add(QStringLiteral("硬件栈帧"), QStringLiteral("R2"), r2, QStringLiteral("函数参数 / 临时变量"));
    add(QStringLiteral("硬件栈帧"), QStringLiteral("R3"), r3, QStringLiteral("函数参数 / 临时变量"));
    add(QStringLiteral("硬件栈帧"), QStringLiteral("R12"), r12, QStringLiteral("IP 寄存器"));
    add(QStringLiteral("硬件栈帧"), QStringLiteral("LR"), lr, QStringLiteral("异常前函数返回地址"));
    add(QStringLiteral("硬件栈帧"), QStringLiteral("PC"), pc, QStringLiteral("异常发生时正在执行的指令地址"));
    add(QStringLiteral("硬件栈帧"), QStringLiteral("xPSR"), xpsr, QStringLiteral("程序状态寄存器"));

    add(QStringLiteral("软件保存"), QStringLiteral("R4"), r4, QStringLiteral("callee-saved"));
    add(QStringLiteral("软件保存"), QStringLiteral("R5"), r5, QStringLiteral("callee-saved"));
    add(QStringLiteral("软件保存"), QStringLiteral("R6"), r6, QStringLiteral("callee-saved"));
    add(QStringLiteral("软件保存"), QStringLiteral("R7"), r7, QStringLiteral("常用作 frame pointer"));
    add(QStringLiteral("软件保存"), QStringLiteral("R8"), r8, QStringLiteral("callee-saved"));
    add(QStringLiteral("软件保存"), QStringLiteral("R9"), r9, QStringLiteral("callee-saved / platform register"));
    add(QStringLiteral("软件保存"), QStringLiteral("R10"), r10, QStringLiteral("callee-saved"));
    add(QStringLiteral("软件保存"), QStringLiteral("R11"), r11, QStringLiteral("常用作 frame pointer"));

    add(QStringLiteral("特殊寄存器"), QStringLiteral("MSP"), msp, QStringLiteral("主堆栈指针"));
    add(QStringLiteral("特殊寄存器"), QStringLiteral("PSP"), psp, QStringLiteral("进程堆栈指针"));
    add(QStringLiteral("特殊寄存器"), QStringLiteral("EXC_RETURN"), excReturn, QStringLiteral("异常返回值"));
    add(QStringLiteral("特殊寄存器"), QStringLiteral("CONTROL"), control, QStringLiteral("控制寄存器"));
    add(QStringLiteral("特殊寄存器"), QStringLiteral("PRIMASK"), primask, QStringLiteral("优先级屏蔽"));
    add(QStringLiteral("特殊寄存器"), QStringLiteral("BASEPRI"), basepri, QStringLiteral("基本优先级屏蔽"));
    add(QStringLiteral("特殊寄存器"), QStringLiteral("FAULTMASK"), faultmask, QStringLiteral("故障屏蔽"));
    add(QStringLiteral("特殊寄存器"), QStringLiteral("faultType"), faultType, ExceptionDumpParser::faultTypeName(faultType));

    add(QStringLiteral("Fault 状态"), QStringLiteral("CFSR"), cfsr, QStringLiteral("MemManage/BusFault/UsageFault 状态"));
    add(QStringLiteral("Fault 状态"), QStringLiteral("HFSR"), hfsr, QStringLiteral("HardFault 状态"));
    add(QStringLiteral("Fault 状态"), QStringLiteral("DFSR"), dfsr, QStringLiteral("Debug Fault 状态"));
    add(QStringLiteral("Fault 状态"), QStringLiteral("AFSR"), afsr, QStringLiteral("Auxiliary Fault 状态"));
    add(QStringLiteral("Fault 状态"), QStringLiteral("MMFAR"), mmfar, QStringLiteral("MemManage fault 地址"));
    add(QStringLiteral("Fault 状态"), QStringLiteral("BFAR"), bfar, QStringLiteral("BusFault 地址"));
    add(QStringLiteral("Fault 状态"), QStringLiteral("SHCSR"), shcsr, QStringLiteral("系统 handler 控制和状态"));
    add(QStringLiteral("Fault 状态"), QStringLiteral("CCR"), ccr, QStringLiteral("配置和控制寄存器"));

    add(QStringLiteral("栈快照"), QStringLiteral("stackStartAddr"), stackStartAddr, QStringLiteral("stackData[0] 对应 MCU 地址"));
    add(QStringLiteral("栈快照"), QStringLiteral("stackSize"), stackSize, QStringLiteral("有效栈快照字节数"));
    add(QStringLiteral("栈快照"), QStringLiteral("stackLimit"), stackLimit, QStringLiteral("栈底低地址"));
    add(QStringLiteral("栈快照"), QStringLiteral("stackTop"), stackTop, QStringLiteral("栈顶高地址"));

    return rows;
}

QVector<quint32> ExceptionRecord::stackWords() const
{
    QVector<quint32> words;
    const int bytes = qMin<int>(stackData.size(), static_cast<int>(qMin<quint32>(stackSize, kStackSnapshotSize)));
    for (int pos = 0; pos + 3 < bytes; pos += 4)
    {
        const uchar *p = reinterpret_cast<const uchar *>(stackData.constData() + pos);
        words.append(static_cast<quint32>(p[0]) |
                     (static_cast<quint32>(p[1]) << 8) |
                     (static_cast<quint32>(p[2]) << 16) |
                     (static_cast<quint32>(p[3]) << 24));
    }
    return words;
}

quint32 ExceptionDumpParser::readU32(const QByteArray &data, qint64 offset)
{
    if (offset < 0 || offset + 4 > data.size())
    {
        return 0u;
    }

    const uchar *p = reinterpret_cast<const uchar *>(data.constData() + offset);
    return static_cast<quint32>(p[0]) |
           (static_cast<quint32>(p[1]) << 8) |
           (static_cast<quint32>(p[2]) << 16) |
           (static_cast<quint32>(p[3]) << 24);
}

quint32 ExceptionDumpParser::crc32(const uchar *data, int length)
{
    quint32 crc = 0xFFFFFFFFu;
    for (int i = 0; i < length; ++i)
    {
        crc ^= static_cast<quint32>(data[i]);
        for (int bit = 0; bit < 8; ++bit)
        {
            if ((crc & 1u) != 0u)
            {
                crc = (crc >> 1) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

ExceptionRecord ExceptionDumpParser::parseOne(const QByteArray &data, qint64 offset, int slotIndex)
{
    ExceptionRecord r;
    r.formatName = QStringLiteral("8KB 异常记录帧 (version[4], stackData[4096], reserved[3900])");
    r.offset = offset;
    r.slotIndex = slotIndex;
    r.slotSize = kSlotSize;
    r.complete = (offset + kSlotSize <= data.size());

    r.magic = readU32(data, offset);
    for (int i = 0; i < 4; ++i)
    {
        r.versionWords.append(readU32(data, offset + 4 + i * 4));
    }
    r.recordIndex = readU32(data, offset + kRecordIndexOffset);
    r.recordState = readU32(data, offset + kRecordStateOffset);

    r.r0 = readU32(data, offset + kHwFrameOffset + 0);
    r.r1 = readU32(data, offset + kHwFrameOffset + 4);
    r.r2 = readU32(data, offset + kHwFrameOffset + 8);
    r.r3 = readU32(data, offset + kHwFrameOffset + 12);
    r.r12 = readU32(data, offset + kHwFrameOffset + 16);
    r.lr = readU32(data, offset + kHwFrameOffset + 20);
    r.pc = readU32(data, offset + kHwFrameOffset + 24);
    r.xpsr = readU32(data, offset + kHwFrameOffset + 28);

    r.r4 = readU32(data, offset + kSwRegsOffset + 0);
    r.r5 = readU32(data, offset + kSwRegsOffset + 4);
    r.r6 = readU32(data, offset + kSwRegsOffset + 8);
    r.r7 = readU32(data, offset + kSwRegsOffset + 12);
    r.r8 = readU32(data, offset + kSwRegsOffset + 16);
    r.r9 = readU32(data, offset + kSwRegsOffset + 20);
    r.r10 = readU32(data, offset + kSwRegsOffset + 24);
    r.r11 = readU32(data, offset + kSwRegsOffset + 28);

    r.msp = readU32(data, offset + kSpecialRegsOffset + 0);
    r.psp = readU32(data, offset + kSpecialRegsOffset + 4);
    r.excReturn = readU32(data, offset + kSpecialRegsOffset + 8);
    r.control = readU32(data, offset + kSpecialRegsOffset + 12);
    r.primask = readU32(data, offset + kSpecialRegsOffset + 16);
    r.basepri = readU32(data, offset + kSpecialRegsOffset + 20);
    r.faultmask = readU32(data, offset + kSpecialRegsOffset + 24);
    r.faultType = readU32(data, offset + kSpecialRegsOffset + 28);

    r.cfsr = readU32(data, offset + kFaultRegsOffset + 0);
    r.hfsr = readU32(data, offset + kFaultRegsOffset + 4);
    r.dfsr = readU32(data, offset + kFaultRegsOffset + 8);
    r.afsr = readU32(data, offset + kFaultRegsOffset + 12);
    r.mmfar = readU32(data, offset + kFaultRegsOffset + 16);
    r.bfar = readU32(data, offset + kFaultRegsOffset + 20);
    r.shcsr = readU32(data, offset + kFaultRegsOffset + 24);
    r.ccr = readU32(data, offset + kFaultRegsOffset + 28);

    r.stackStartAddr = readU32(data, offset + kStackInfoOffset + 0);
    r.stackSize = readU32(data, offset + kStackInfoOffset + 4);
    r.stackLimit = readU32(data, offset + kStackInfoOffset + 8);
    r.stackTop = readU32(data, offset + kStackInfoOffset + 12);

    const qint64 stackOffset = offset + kStackDataOffset;
    const int availableStackBytes = qMax<qint64>(0, qMin<qint64>(kStackSnapshotSize, data.size() - stackOffset));
    r.stackData = data.mid(static_cast<int>(stackOffset), availableStackBytes);

    if (offset + kCrcOffset + 4 <= data.size())
    {
        r.crcAvailable = true;
        r.storedCrc = readU32(data, offset + kCrcOffset);
        r.computedCrc = crc32(reinterpret_cast<const uchar *>(data.constData() + offset), kSlotSize - 4);
        r.crcOk = (r.storedCrc == r.computedCrc);
    }

    r.warnings = stackSanityWarnings(r);
    if (r.magic != kMagic)
    {
        r.warnings << QStringLiteral("magic 不匹配。");
    }
    if (r.recordState != kStateValid)
    {
        r.warnings << QStringLiteral("recordState 不是 VALID。");
    }
    if (r.crcAvailable && !r.crcOk)
    {
        r.warnings << QStringLiteral("CRC32 校验失败。");
    }
    if (!r.complete)
    {
        r.warnings << QStringLiteral("记录长度不足一个完整 slot，可能是截断文件。");
    }

    return r;
}

QVector<ExceptionRecord> ExceptionDumpParser::parseRecords(const QByteArray &data, bool scanForMagic)
{
    QVector<ExceptionRecord> records;
    QSet<qint64> seenOffsets;

    if (data.size() < kHeaderSize)
    {
        return records;
    }

    const int slotCount = qMax(1, (data.size() + kSlotSize - 1) / kSlotSize);
    for (int slot = 0; slot < slotCount; ++slot)
    {
        const qint64 offset = static_cast<qint64>(slot) * kSlotSize;
        if (offset + kHeaderSize > data.size())
        {
            continue;
        }
        if (readU32(data, offset) == kMagic)
        {
            records.append(parseOne(data, offset, slot));
            seenOffsets.insert(offset);
        }
    }

    if (scanForMagic)
    {
        for (qint64 offset = 0; offset + kHeaderSize <= data.size(); offset += 4)
        {
            if (seenOffsets.contains(offset))
            {
                continue;
            }
            if (readU32(data, offset) == kMagic)
            {
                records.append(parseOne(data, offset, records.size()));
                seenOffsets.insert(offset);
            }
        }
    }

    std::sort(records.begin(), records.end(), [](const ExceptionRecord &a, const ExceptionRecord &b) {
        if (a.recordState == kStateValid && b.recordState != kStateValid)
        {
            return true;
        }
        if (a.recordState != kStateValid && b.recordState == kStateValid)
        {
            return false;
        }
        if (a.recordIndex != b.recordIndex)
        {
            return a.recordIndex > b.recordIndex;
        }
        return a.offset < b.offset;
    });

    return records;
}

ParseResult ExceptionDumpParser::parseFile(const QString &path, QString *errorMessage) const
{
    ParseResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = file.errorString();
        }
        return result;
    }

    const QByteArray data = file.readAll();
    result.messages << QStringLiteral("读取文件：%1，大小 %2 bytes").arg(QDir::toNativeSeparators(path)).arg(data.size());

    result.records = parseRecords(data, false);
    result.chosenFormat = QStringLiteral("8KB 异常记录帧 (version[4], stackData[4096], reserved[3900])");

    if (result.records.isEmpty())
    {
        result.records = parseRecords(data, true);
        result.messages << QStringLiteral("未在 slot 起始处找到 magic，已改用 4 字节对齐扫描。");
    }

    if (result.records.isEmpty())
    {
        result.messages << QStringLiteral("没有找到 magic=0x45584350 的异常记录。");
    }
    else
    {
        result.messages << QStringLiteral("选择解析格式：%1，找到 %2 条候选记录。")
                               .arg(result.chosenFormat)
                               .arg(result.records.size());
    }

    return result;
}
