#ifndef SYMBOL_RESOLVER_H
#define SYMBOL_RESOLVER_H

#include <QtCore>

struct SymbolRange
{
    quint32 start = 0;
    quint32 end = 0;
    QString name;
    QString object;
};

struct SourceLocation
{
    quint32 inputAddress = 0;
    quint32 lookupAddress = 0;
    QString function;
    QString fileLine;
    QString mapSymbol;
    QString object;
    bool fromAddr2Line = false;
    bool fromMap = false;
    bool valid = false;

    QString displayFunction() const;
    QString displayFileLine() const;
};

class SymbolResolver
{
public:
    void setElfPath(const QString &path);
    void setMapPath(const QString &path);

    bool loadMap(QString *errorMessage);
    bool hasMap() const;
    QString mapStatus() const;

    SourceLocation resolve(quint32 address, bool isReturnAddress) const;
    bool isKnownCodeAddress(quint32 address, bool isReturnAddress) const;

    QString addr2linePath() const;
    QString toolStatus() const;

private:
    QString m_elfPath;
    QString m_mapPath;
    QVector<SymbolRange> m_symbols;
    mutable QHash<QString, SourceLocation> m_cache;

    static quint32 normalizeAddress(quint32 address, bool isReturnAddress);
    static QString hexAddress(quint32 address);
    static QString findExecutable(const QString &name, const QStringList &extraDirs = QStringList());
    const SymbolRange *findSymbol(quint32 address) const;
    SourceLocation resolveByAddr2Line(quint32 inputAddress, quint32 lookupAddress) const;
    SourceLocation resolveByMap(quint32 inputAddress, quint32 lookupAddress) const;
};

#endif
