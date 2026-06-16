#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QtWidgets>

#include "core/exception_record.h"
#include "core/symbol_resolver.h"

class CanTransferWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void browseBin();
    void browseElf();
    void browseMap();
    void analyze();
    void onRecordChanged(int index);
    void onTraceStrategyChanged();
    void onCanBinarySaved(const QString &path);
    void onCanBinarySavedAndAnalyze(const QString &path);

private:
    struct TraceRow
    {
        QString source;
        quint32 inputAddress = 0;
        SourceLocation location;
        QString note;
    };

    struct FramePointerRecord
    {
        quint32 recordAddress = 0;
        quint32 previousFramePointer = 0;
        quint32 returnAddress = 0;
        QString layout;
    };

    QLineEdit *m_binEdit = nullptr;
    QLineEdit *m_elfEdit = nullptr;
    QLineEdit *m_mapEdit = nullptr;
    QCheckBox *m_framePointerCheckBox = nullptr;
    QPushButton *m_analyzeButton = nullptr;
    QComboBox *m_recordCombo = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPlainTextEdit *m_summaryEdit = nullptr;
    QTableWidget *m_registerTable = nullptr;
    QPlainTextEdit *m_faultEdit = nullptr;
    QTableWidget *m_traceTable = nullptr;
    QTableWidget *m_stackTable = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;
    QTabWidget *m_topTabs = nullptr;
    CanTransferWidget *m_canWidget = nullptr;

    ParseResult m_parseResult;
    SymbolResolver m_resolver;

    QWidget *createFilePanel();
    QWidget *createCentralPanel();
    QWidget *createAnalysisPanel();
    QLineEdit *addFileRow(QGridLayout *layout, int row, const QString &label, const QString &filter, const char *slot);

    void populateRecords();
    void updateParsedSlotStatus();
    void showRecord(int recordIndex);
    QString buildSummary(const ExceptionRecord &record, const SourceLocation &pcLoc, const SourceLocation &lrLoc) const;
    QVector<TraceRow> buildTrace(const ExceptionRecord &record);
    int appendFramePointerTrace(const ExceptionRecord &record, QVector<TraceRow> *rows, QSet<quint32> *seen);
    void appendStackScanTrace(const ExceptionRecord &record, QVector<TraceRow> *rows, QSet<quint32> *seen);
    void addTraceAddress(QVector<TraceRow> *rows, QSet<quint32> *seen, const QString &source, quint32 address, bool isReturnAddress, const QString &note);
    bool findFramePointerRecord(const ExceptionRecord &record, quint32 framePointer, FramePointerRecord *frameRecord) const;
    bool readStackWord(const ExceptionRecord &record, quint32 address, quint32 *value) const;
    bool isAddressInStackSnapshot(const ExceptionRecord &record, quint32 address, quint32 width = 4) const;
    bool isValidNextFramePointer(const ExceptionRecord &record, quint32 currentFramePointer, quint32 nextFramePointer) const;
    bool isLikelyReturnAddress(quint32 value) const;
    void fillRegisterTable(const ExceptionRecord &record);
    void fillFaultView(const ExceptionRecord &record);
    void fillTraceTable(const QVector<TraceRow> &rows);
    void fillStackTable(const ExceptionRecord &record);
    void appendLog(const QString &line);

    static QString defaultPath(const QString &relative);
    static QString hex32(quint32 value);
    static bool isUsefulStackWord(quint32 value);
};

#endif
