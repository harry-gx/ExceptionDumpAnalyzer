#include "core/symbol_resolver.h"

#include <algorithm>

QString SourceLocation::displayFunction() const
{
    if (!function.isEmpty() && function != QStringLiteral("??"))
    {
        return function;
    }
    if (!mapSymbol.isEmpty())
    {
        return mapSymbol;
    }
    return QStringLiteral("??");
}

QString SourceLocation::displayFileLine() const
{
    if (!fileLine.isEmpty() && fileLine != QStringLiteral("??:0") && fileLine != QStringLiteral("??"))
    {
        return fileLine;
    }
    if (!object.isEmpty())
    {
        return object;
    }
    return QStringLiteral("??");
}

void SymbolResolver::setElfPath(const QString &path)
{
    m_elfPath = path;
    m_cache.clear();
}

void SymbolResolver::setMapPath(const QString &path)
{
    m_mapPath = path;
    m_symbols.clear();
    m_cache.clear();
}

bool SymbolResolver::hasMap() const
{
    return !m_symbols.isEmpty();
}

QString SymbolResolver::mapStatus() const
{
    if (m_mapPath.isEmpty())
    {
        return QStringLiteral("未选择 MAP 文件");
    }
    return QStringLiteral("MAP 符号数：%1").arg(m_symbols.size());
}

QString SymbolResolver::hexAddress(quint32 address)
{
    return QStringLiteral("0x%1").arg(address, 8, 16, QLatin1Char('0')).toUpper();
}

QString SymbolResolver::findExecutable(const QString &name, const QStringList &extraDirs)
{
    QStringList dirs = extraDirs;
    const QString pathEnv = QString::fromLocal8Bit(qgetenv("PATH"));
    dirs.append(pathEnv.split(QDir::listSeparator(), QString::SkipEmptyParts));

    for (const QString &dir : dirs)
    {
        const QString candidate = QDir(dir).filePath(name);
        if (QFileInfo(candidate).isFile())
        {
            return QDir::toNativeSeparators(candidate);
        }
    }
    return QString();
}

QString SymbolResolver::addr2linePath() const
{
    QStringList extraDirs;
    const QDir appDir(QCoreApplication::applicationDirPath());
    extraDirs << appDir.filePath(QStringLiteral("tools/arm-none-eabi/bin"))
              << appDir.filePath(QStringLiteral("tools"))
              << QStringLiteral("D:/D-install/arm-none-eabi/14.3 rel1/bin")
              << QStringLiteral("C:/NXP/S32DS_ARM_v2.2/S32DS/build_tools/gcc_v4.9/gcc-arm-none-eabi-4_9/bin");
    return findExecutable(QStringLiteral("arm-none-eabi-addr2line.exe"), extraDirs);
}

QString SymbolResolver::toolStatus() const
{
    const QString tool = addr2linePath();
    if (tool.isEmpty())
    {
        return QStringLiteral("未找到 arm-none-eabi-addr2line，将只使用 MAP 符号。");
    }
    return QStringLiteral("addr2line：%1").arg(tool);
}

quint32 SymbolResolver::normalizeAddress(quint32 address, bool isReturnAddress)
{
    quint32 normalized = address & ~1u;
    if (isReturnAddress && normalized >= 2u)
    {
        normalized -= 2u;
    }
    return normalized;
}

bool SymbolResolver::loadMap(QString *errorMessage)
{
    m_symbols.clear();
    m_cache.clear();

    if (m_mapPath.isEmpty())
    {
        return true;
    }

    QFile file(m_mapPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream in(&file);
    QRegExp memoryMapStartRe(QStringLiteral("^Linker script and memory map"));
    QRegExp textSectionRe(QStringLiteral("^\\s*\\.text(?:\\.([^\\s]+))?\\s+0x([0-9A-Fa-f]+)\\s+0x([0-9A-Fa-f]+)\\s*(.*)$"));
    QRegExp textSectionNameOnlyRe(QStringLiteral("^\\s*\\.text\\.([^\\s]+)\\s*$"));
    QRegExp textSectionContinuationRe(QStringLiteral("^\\s+0x([0-9A-Fa-f]+)\\s+0x([0-9A-Fa-f]+)\\s*(.*)$"));
    QRegExp symbolLineRe(QStringLiteral("^\\s*0x([0-9A-Fa-f]+)\\s+([A-Za-z_.$][A-Za-z0-9_.$@]*)\\s*$"));
    bool inMemoryMap = false;
    QString pendingTextName;

    while (!in.atEnd())
    {
        const QString line = in.readLine();

        if (!inMemoryMap)
        {
            if (memoryMapStartRe.indexIn(line) == 0)
            {
                inMemoryMap = true;
            }
            continue;
        }

        if (textSectionRe.indexIn(line) == 0)
        {
            pendingTextName.clear();
            const QString suffix = textSectionRe.cap(1).trimmed();
            const bool okName = !suffix.isEmpty() && suffix != QStringLiteral("*");
            if (okName)
            {
                bool okStart = false;
                bool okSize = false;
                SymbolRange range;
                range.name = suffix;
                range.start = textSectionRe.cap(2).toUInt(&okStart, 16);
                const quint32 size = textSectionRe.cap(3).toUInt(&okSize, 16);
                range.end = range.start + size;
                range.object = textSectionRe.cap(4).trimmed();
                if (okStart && okSize && size > 0u)
                {
                    m_symbols.append(range);
                }
            }
            continue;
        }

        if (textSectionNameOnlyRe.indexIn(line) == 0)
        {
            pendingTextName = textSectionNameOnlyRe.cap(1).trimmed();
            continue;
        }

        if (!pendingTextName.isEmpty() && textSectionContinuationRe.indexIn(line) == 0)
        {
            bool okStart = false;
            bool okSize = false;
            SymbolRange range;
            range.name = pendingTextName;
            range.start = textSectionContinuationRe.cap(1).toUInt(&okStart, 16);
            const quint32 size = textSectionContinuationRe.cap(2).toUInt(&okSize, 16);
            range.end = range.start + size;
            range.object = textSectionContinuationRe.cap(3).trimmed();
            if (okStart && okSize && size > 0u)
            {
                m_symbols.append(range);
            }
            pendingTextName.clear();
            continue;
        }

        pendingTextName.clear();

        if (symbolLineRe.indexIn(line) == 0)
        {
            bool okStart = false;
            const quint32 start = symbolLineRe.cap(1).toUInt(&okStart, 16);
            const QString name = symbolLineRe.cap(2).trimmed();
            if (okStart && !name.isEmpty())
            {
                SymbolRange range;
                range.start = start;
                range.end = start;
                range.name = name;
                m_symbols.append(range);
            }
        }
    }

    std::sort(m_symbols.begin(), m_symbols.end(), [](const SymbolRange &a, const SymbolRange &b) {
        if (a.start != b.start)
        {
            return a.start < b.start;
        }
        const quint32 as = (a.end > a.start) ? (a.end - a.start) : 0u;
        const quint32 bs = (b.end > b.start) ? (b.end - b.start) : 0u;
        return as > bs;
    });

    for (int i = 0; i < m_symbols.size(); ++i)
    {
        if (m_symbols[i].end > m_symbols[i].start)
        {
            continue;
        }

        quint32 nextStart = m_symbols[i].start + 2u;
        for (int j = i + 1; j < m_symbols.size(); ++j)
        {
            if (m_symbols[j].start > m_symbols[i].start)
            {
                nextStart = m_symbols[j].start;
                break;
            }
        }
        m_symbols[i].end = nextStart;
    }

    return true;
}

const SymbolRange *SymbolResolver::findSymbol(quint32 address) const
{
    const SymbolRange *best = nullptr;
    quint32 bestSize = 0xFFFFFFFFu;

    for (const SymbolRange &symbol : m_symbols)
    {
        if (address < symbol.start || address >= symbol.end)
        {
            continue;
        }

        const quint32 size = symbol.end - symbol.start;
        if (best == nullptr || size < bestSize)
        {
            best = &symbol;
            bestSize = size;
        }
    }

    return best;
}

SourceLocation SymbolResolver::resolveByMap(quint32 inputAddress, quint32 lookupAddress) const
{
    SourceLocation loc;
    loc.inputAddress = inputAddress;
    loc.lookupAddress = lookupAddress;

    const SymbolRange *symbol = findSymbol(lookupAddress);
    if (symbol != nullptr)
    {
        loc.mapSymbol = symbol->name;
        loc.object = symbol->object;
        loc.fromMap = true;
        loc.valid = true;
    }

    return loc;
}

SourceLocation SymbolResolver::resolveByAddr2Line(quint32 inputAddress, quint32 lookupAddress) const
{
    SourceLocation loc;
    loc.inputAddress = inputAddress;
    loc.lookupAddress = lookupAddress;

    const QString tool = addr2linePath();
    if (tool.isEmpty() || m_elfPath.isEmpty() || !QFileInfo(m_elfPath).isFile())
    {
        return loc;
    }

    QProcess process;
    QStringList args;
    args << QStringLiteral("-e") << m_elfPath
         << QStringLiteral("-f")
         << QStringLiteral("-C")
         << QStringLiteral("-a")
         << hexAddress(lookupAddress);
    process.start(tool, args);
    if (!process.waitForStarted(2000))
    {
        return loc;
    }
    if (!process.waitForFinished(5000))
    {
        process.kill();
        process.waitForFinished(1000);
        return loc;
    }

    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    const QStringList lines = output.split(QRegExp(QStringLiteral("[\\r\\n]+")), QString::SkipEmptyParts);
    if (lines.size() >= 3)
    {
        loc.function = lines.at(1).trimmed();
        loc.fileLine = lines.at(2).trimmed();
        loc.fromAddr2Line = true;
        if (loc.function != QStringLiteral("??") || (loc.fileLine != QStringLiteral("??:0") && loc.fileLine != QStringLiteral("??")))
        {
            loc.valid = true;
        }
    }

    return loc;
}

SourceLocation SymbolResolver::resolve(quint32 address, bool isReturnAddress) const
{
    const quint32 lookup = normalizeAddress(address, isReturnAddress);
    const QString key = QStringLiteral("%1:%2").arg(lookup).arg(isReturnAddress ? 1 : 0);
    if (m_cache.contains(key))
    {
        return m_cache.value(key);
    }

    SourceLocation loc = resolveByAddr2Line(address, lookup);
    SourceLocation mapLoc = resolveByMap(address, lookup);

    if (loc.mapSymbol.isEmpty())
    {
        loc.mapSymbol = mapLoc.mapSymbol;
    }
    if (loc.object.isEmpty())
    {
        loc.object = mapLoc.object;
    }
    loc.fromMap = mapLoc.fromMap;
    loc.valid = loc.valid || mapLoc.valid;
    loc.inputAddress = address;
    loc.lookupAddress = lookup;

    m_cache.insert(key, loc);
    return loc;
}

bool SymbolResolver::isKnownCodeAddress(quint32 address, bool isReturnAddress) const
{
    const quint32 lookup = normalizeAddress(address, isReturnAddress);
    if (findSymbol(lookup) != nullptr)
    {
        return true;
    }

    if (m_symbols.isEmpty())
    {
        return (lookup >= 0x00000100u && lookup < 0x01000000u);
    }

    return false;
}
