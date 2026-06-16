#include "ui/main_window.h"

#include "can/can_transfer_widget.h"

namespace
{
QString nativePath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

QTableWidgetItem *item(const QString &text)
{
    QTableWidgetItem *it = new QTableWidgetItem(text);
    it->setFlags(it->flags() ^ Qt::ItemIsEditable);
    return it;
}

QString asciiFromU32(quint32 value)
{
    QString text;
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        const char ch = static_cast<char>((value >> shift) & 0xFFu);
        text.append((ch >= 0x20 && ch <= 0x7E) ? QChar::fromLatin1(ch) : QLatin1Char('.'));
    }
    return text;
}

QString hex32WithAscii(quint32 value)
{
    return QStringLiteral("%1（%2）").arg(ExceptionDumpParser::hex32(value), asciiFromU32(value));
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("异常现场解析与回溯工具"));
    setWindowIcon(QApplication::windowIcon());
    resize(1180, 760);
    setCentralWidget(createCentralPanel());

    m_binEdit->setText(defaultPath(QStringLiteral("../test.bin")));
    m_elfEdit->setText(defaultPath(QStringLiteral("../../FBL_exception_protect/Debug/FBL_exception_protect.elf")));
    m_mapEdit->setText(defaultPath(QStringLiteral("../../FBL_exception_protect/Debug/FBL_exception_protect.map")));
    m_statusLabel->setText(QStringLiteral("请选择 test.bin、ELF 和 MAP 后点击解析。"));
}

QWidget *MainWindow::createCentralPanel()
{
    m_topTabs = new QTabWidget(this);
    m_topTabs->addTab(createAnalysisPanel(), QStringLiteral("异常解析"));

    m_canWidget = new CanTransferWidget(m_topTabs);
    m_topTabs->addTab(m_canWidget, QStringLiteral("CAN收发"));

    connect(m_canWidget, &CanTransferWidget::binarySaved, this, &MainWindow::onCanBinarySaved);
    connect(m_canWidget, &CanTransferWidget::binarySavedAndAnalyze, this, &MainWindow::onCanBinarySavedAndAnalyze);

    return m_topTabs;
}

QWidget *MainWindow::createAnalysisPanel()
{
    QWidget *root = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(root);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    layout->addWidget(createFilePanel());

    QHBoxLayout *recordLayout = new QHBoxLayout;
    recordLayout->addWidget(new QLabel(QStringLiteral("异常记录")));
    m_recordCombo = new QComboBox(root);
    m_recordCombo->setMinimumWidth(360);
    connect(m_recordCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onRecordChanged(int)));
    recordLayout->addWidget(m_recordCombo);
    m_statusLabel = new QLabel(root);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    recordLayout->addWidget(m_statusLabel, 1);
    layout->addLayout(recordLayout);

    QTabWidget *tabs = new QTabWidget(root);
    m_summaryEdit = new QPlainTextEdit(tabs);
    m_summaryEdit->setReadOnly(true);
    m_summaryEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    tabs->addTab(m_summaryEdit, QStringLiteral("异常概览"));

    m_registerTable = new QTableWidget(tabs);
    m_registerTable->setColumnCount(4);
    m_registerTable->setHorizontalHeaderLabels(QStringList() << QStringLiteral("分组") << QStringLiteral("寄存器") << QStringLiteral("值") << QStringLiteral("说明"));
    m_registerTable->horizontalHeader()->setStretchLastSection(true);
    m_registerTable->verticalHeader()->setVisible(false);
    m_registerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabs->addTab(m_registerTable, QStringLiteral("寄存器"));

    m_faultEdit = new QPlainTextEdit(tabs);
    m_faultEdit->setReadOnly(true);
    m_faultEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    tabs->addTab(m_faultEdit, QStringLiteral("Fault 解码"));

    m_traceTable = new QTableWidget(tabs);
    m_traceTable->setColumnCount(6);
    m_traceTable->setHorizontalHeaderLabels(QStringList() << QStringLiteral("#") << QStringLiteral("来源") << QStringLiteral("地址") << QStringLiteral("函数") << QStringLiteral("源码 / 对象") << QStringLiteral("说明"));
    m_traceTable->horizontalHeader()->setStretchLastSection(true);
    m_traceTable->verticalHeader()->setVisible(false);
    m_traceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabs->addTab(m_traceTable, QStringLiteral("调用链"));

    m_stackTable = new QTableWidget(tabs);
    m_stackTable->setColumnCount(4);
    m_stackTable->setHorizontalHeaderLabels(QStringList() << QStringLiteral("偏移") << QStringLiteral("MCU 地址") << QStringLiteral("值") << QStringLiteral("符号"));
    m_stackTable->horizontalHeader()->setStretchLastSection(true);
    m_stackTable->verticalHeader()->setVisible(false);
    m_stackTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabs->addTab(m_stackTable, QStringLiteral("栈快照"));

    m_logEdit = new QPlainTextEdit(tabs);
    m_logEdit->setReadOnly(true);
    m_logEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    tabs->addTab(m_logEdit, QStringLiteral("日志"));

    layout->addWidget(tabs, 1);
    return root;
}

QWidget *MainWindow::createFilePanel()
{
    QWidget *panel = new QWidget(this);
    QGridLayout *grid = new QGridLayout(panel);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);

    m_binEdit = addFileRow(grid, 0, QStringLiteral("二进制"), QStringLiteral("Binary (*.bin);;All files (*.*)"), SLOT(browseBin()));
    m_elfEdit = addFileRow(grid, 1, QStringLiteral("ELF"), QStringLiteral("ELF (*.elf);;All files (*.*)"), SLOT(browseElf()));
    m_mapEdit = addFileRow(grid, 2, QStringLiteral("MAP"), QStringLiteral("Map (*.map);;All files (*.*)"), SLOT(browseMap()));

    m_framePointerCheckBox = new QCheckBox(QStringLiteral("使用 Frame Pointer(R7) 回溯"), panel);
    m_framePointerCheckBox->setChecked(false);
    m_framePointerCheckBox->setToolTip(QStringLiteral("勾选后只按保存的 R7 栈帧链回溯；不勾选时只扫描栈快照中的疑似返回地址。"));
    connect(m_framePointerCheckBox, SIGNAL(toggled(bool)), this, SLOT(onTraceStrategyChanged()));
    grid->addWidget(m_framePointerCheckBox, 3, 1, 1, 2);

    m_analyzeButton = new QPushButton(QStringLiteral("解析并回溯"), panel);
    connect(m_analyzeButton, SIGNAL(clicked()), this, SLOT(analyze()));
    grid->addWidget(m_analyzeButton, 0, 3, 4, 1);

    grid->setColumnStretch(1, 1);
    return panel;
}

QLineEdit *MainWindow::addFileRow(QGridLayout *layout, int row, const QString &label, const QString &filter, const char *slot)
{
    Q_UNUSED(filter);
    QLabel *text = new QLabel(label, this);
    QLineEdit *edit = new QLineEdit(this);
    edit->setMinimumWidth(520);
    QPushButton *button = new QPushButton(QStringLiteral("浏览"), this);
    connect(button, SIGNAL(clicked()), this, slot);

    layout->addWidget(text, row, 0);
    layout->addWidget(edit, row, 1);
    layout->addWidget(button, row, 2);
    return edit;
}

QString MainWindow::defaultPath(const QString &relative)
{
    QDir dir(QCoreApplication::applicationDirPath());
    if (dir.dirName().compare(QStringLiteral("debug"), Qt::CaseInsensitive) == 0 ||
        dir.dirName().compare(QStringLiteral("release"), Qt::CaseInsensitive) == 0)
    {
        dir.cdUp();
    }
    return nativePath(dir.absoluteFilePath(relative));
}

QString MainWindow::hex32(quint32 value)
{
    return ExceptionDumpParser::hex32(value);
}

void MainWindow::browseBin()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择异常二进制文件"), QFileInfo(m_binEdit->text()).absolutePath(), QStringLiteral("Binary (*.bin);;All files (*.*)"));
    if (!path.isEmpty())
    {
        m_binEdit->setText(nativePath(path));
    }
}

void MainWindow::browseElf()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 ELF 文件"), QFileInfo(m_elfEdit->text()).absolutePath(), QStringLiteral("ELF (*.elf);;All files (*.*)"));
    if (!path.isEmpty())
    {
        m_elfEdit->setText(nativePath(path));
    }
}

void MainWindow::browseMap()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 MAP 文件"), QFileInfo(m_mapEdit->text()).absolutePath(), QStringLiteral("Map (*.map);;All files (*.*)"));
    if (!path.isEmpty())
    {
        m_mapEdit->setText(nativePath(path));
    }
}

void MainWindow::onCanBinarySaved(const QString &path)
{
    const QString binPath = nativePath(path);
    m_binEdit->setText(binPath);
    m_statusLabel->setText(QStringLiteral("已保存 CAN 接收数据：%1").arg(binPath));
}

void MainWindow::onCanBinarySavedAndAnalyze(const QString &path)
{
    onCanBinarySaved(path);
    if (m_topTabs)
    {
        m_topTabs->setCurrentIndex(0);
    }
    analyze();
}

void MainWindow::appendLog(const QString &line)
{
    m_logEdit->appendPlainText(line);
}

void MainWindow::analyze()
{
    m_logEdit->clear();
    m_summaryEdit->clear();
    m_faultEdit->clear();
    m_registerTable->setRowCount(0);
    m_traceTable->setRowCount(0);
    m_stackTable->setRowCount(0);
    m_recordCombo->clear();

    const QString binPath = m_binEdit->text().trimmed();
    if (binPath.isEmpty() || !QFileInfo(binPath).isFile())
    {
        QMessageBox::warning(this, QStringLiteral("输入错误"), QStringLiteral("请选择有效的二进制文件。"));
        return;
    }

    m_resolver.setElfPath(m_elfEdit->text().trimmed());
    m_resolver.setMapPath(m_mapEdit->text().trimmed());

    QString mapError;
    if (!m_resolver.loadMap(&mapError))
    {
        appendLog(QStringLiteral("MAP 读取失败：%1").arg(mapError));
    }
    appendLog(m_resolver.toolStatus());
    appendLog(m_resolver.mapStatus());

    ExceptionDumpParser parser;
    QString error;
    m_parseResult = parser.parseFile(binPath, &error);
    if (!error.isEmpty())
    {
        QMessageBox::critical(this, QStringLiteral("读取失败"), error);
        return;
    }
    for (const QString &message : m_parseResult.messages)
    {
        appendLog(message);
    }

    populateRecords();
    updateParsedSlotStatus();
    if (!m_parseResult.records.isEmpty())
    {
        m_recordCombo->setCurrentIndex(0);
        showRecord(0);
    }
}

void MainWindow::populateRecords()
{
    m_recordCombo->blockSignals(true);
    m_recordCombo->clear();

    for (int i = 0; i < m_parseResult.records.size(); ++i)
    {
        const ExceptionRecord &record = m_parseResult.records.at(i);
        QString text = QStringLiteral("#%1  slot=%2  offset=%3  state=%4  crc=%5")
                           .arg(record.recordIndex)
                           .arg(record.slotIndex)
                           .arg(hex32(static_cast<quint32>(record.offset)))
                           .arg(ExceptionDumpParser::stateName(record.recordState))
                           .arg(record.crcAvailable ? (record.crcOk ? QStringLiteral("OK") : QStringLiteral("FAIL")) : QStringLiteral("N/A"));
        m_recordCombo->addItem(text, i);
    }

    m_recordCombo->blockSignals(false);
}

void MainWindow::updateParsedSlotStatus()
{
    m_statusLabel->setText(QStringLiteral("共解析到 %1 个 slot 数据").arg(m_parseResult.records.size()));
}

void MainWindow::onRecordChanged(int index)
{
    if (index >= 0)
    {
        showRecord(index);
    }
}

void MainWindow::onTraceStrategyChanged()
{
    const int index = m_recordCombo->currentIndex();
    if (index >= 0 && !m_parseResult.records.isEmpty())
    {
        showRecord(index);
    }
}

QString MainWindow::buildSummary(const ExceptionRecord &record, const SourceLocation &pcLoc, const SourceLocation &lrLoc) const
{
    QStringList lines;
    lines << QStringLiteral("解析格式      : %1").arg(record.formatName);
    lines << QStringLiteral("Slot          : %1").arg(record.slotIndex);
    lines << QStringLiteral("文件偏移      : %1").arg(hex32(static_cast<quint32>(record.offset)));
    lines << QStringLiteral("Magic         : %1").arg(hex32WithAscii(record.magic));
    lines << QStringLiteral("Version       : %1").arg([&record]() {
        QStringList parts;
        for (quint32 word : record.versionWords)
        {
            parts << hex32WithAscii(word);
        }
        return parts.join(QStringLiteral(", "));
    }());
    lines << QStringLiteral("RecordIndex   : %1").arg(record.recordIndex);
    lines << QStringLiteral("RecordState   : %1 (%2)").arg(hex32(record.recordState), ExceptionDumpParser::stateName(record.recordState));
    lines << QStringLiteral("CRC32         : %1 stored=%2 computed=%3")
                 .arg(record.crcAvailable ? (record.crcOk ? QStringLiteral("OK") : QStringLiteral("FAIL")) : QStringLiteral("N/A"),
                      hex32(record.storedCrc),
                      hex32(record.computedCrc));
    lines << QString();
    lines << QStringLiteral("异常类型      : %1 (%2)").arg(ExceptionDumpParser::faultTypeName(record.faultType)).arg(record.faultType);
    lines << QStringLiteral("PC            : %1  %2  %3").arg(hex32(record.pc), pcLoc.displayFunction(), pcLoc.displayFileLine());
    lines << QStringLiteral("LR            : %1  %2  %3").arg(hex32(record.lr), lrLoc.displayFunction(), lrLoc.displayFileLine());
    lines << QStringLiteral("EXC_RETURN    : %1  %2").arg(hex32(record.excReturn), ExceptionDumpParser::excReturnDescription(record.excReturn));
    lines << QStringLiteral("CFSR/HFSR/DFSR: %1 / %2 / %3").arg(hex32(record.cfsr), hex32(record.hfsr), hex32(record.dfsr));
    lines << QStringLiteral("MMFAR/BFAR    : %1 / %2").arg(hex32(record.mmfar), hex32(record.bfar));
    lines << QStringLiteral("Stack         : start=%1 size=%2 limit=%3 top=%4")
                 .arg(hex32(record.stackStartAddr))
                 .arg(record.stackSize)
                 .arg(hex32(record.stackLimit))
                 .arg(hex32(record.stackTop));

    if (!record.warnings.isEmpty())
    {
        lines << QString();
        lines << QStringLiteral("警告");
        for (const QString &warning : record.warnings)
        {
            lines << QStringLiteral("  - %1").arg(warning);
        }
    }

    return lines.join(QStringLiteral("\n"));
}

void MainWindow::showRecord(int recordIndex)
{
    if (recordIndex < 0 || recordIndex >= m_parseResult.records.size())
    {
        return;
    }

    const ExceptionRecord &record = m_parseResult.records.at(recordIndex);
    const SourceLocation pcLoc = m_resolver.resolve(record.pc, false);
    const SourceLocation lrLoc = m_resolver.resolve(record.lr, true);

    updateParsedSlotStatus();

    m_summaryEdit->setPlainText(buildSummary(record, pcLoc, lrLoc));
    fillRegisterTable(record);
    fillFaultView(record);
    fillTraceTable(buildTrace(record));
    fillStackTable(record);
}

bool MainWindow::isUsefulStackWord(quint32 value)
{
    if (value == 0u || value == 0xFFFFFFFFu)
    {
        return false;
    }
    if ((value & 1u) == 0u)
    {
        return false;
    }
    return true;
}

bool MainWindow::isLikelyReturnAddress(quint32 value) const
{
    return isUsefulStackWord(value) && m_resolver.isKnownCodeAddress(value, true);
}

bool MainWindow::isAddressInStackSnapshot(const ExceptionRecord &record, quint32 address, quint32 width) const
{
    if (width == 0u || record.stackSize == 0u || record.stackData.isEmpty())
    {
        return false;
    }

    const quint32 validBytes = qMin<quint32>(record.stackSize, static_cast<quint32>(record.stackData.size()));
    if (validBytes < width || address < record.stackStartAddr)
    {
        return false;
    }

    const quint64 offset = static_cast<quint64>(address) - static_cast<quint64>(record.stackStartAddr);
    return (offset + width) <= validBytes;
}

bool MainWindow::readStackWord(const ExceptionRecord &record, quint32 address, quint32 *value) const
{
    if (value == nullptr || (address & 0x3u) != 0u || !isAddressInStackSnapshot(record, address, 4u))
    {
        return false;
    }

    const quint32 offset = address - record.stackStartAddr;
    const uchar *p = reinterpret_cast<const uchar *>(record.stackData.constData() + offset);
    *value = static_cast<quint32>(p[0]) |
             (static_cast<quint32>(p[1]) << 8) |
             (static_cast<quint32>(p[2]) << 16) |
             (static_cast<quint32>(p[3]) << 24);
    return true;
}

bool MainWindow::isValidNextFramePointer(const ExceptionRecord &record, quint32 currentFramePointer, quint32 nextFramePointer) const
{
    if (nextFramePointer == 0u || nextFramePointer == 0xFFFFFFFFu)
    {
        return true;
    }
    if ((nextFramePointer & 0x3u) != 0u)
    {
        return false;
    }
    if (nextFramePointer <= currentFramePointer)
    {
        return false;
    }
    return isAddressInStackSnapshot(record, nextFramePointer, 4u);
}

bool MainWindow::findFramePointerRecord(const ExceptionRecord &record, quint32 framePointer, FramePointerRecord *frameRecord) const
{
    if (frameRecord == nullptr || (framePointer & 0x3u) != 0u ||
        !isAddressInStackSnapshot(record, framePointer, 8u))
    {
        return false;
    }

    quint32 previousFramePointer = 0u;
    quint32 returnAddress = 0u;
    if (readStackWord(record, framePointer, &previousFramePointer) &&
        readStackWord(record, framePointer + 4u, &returnAddress) &&
        isValidNextFramePointer(record, framePointer, previousFramePointer) &&
        isLikelyReturnAddress(returnAddress))
    {
        frameRecord->recordAddress = framePointer;
        frameRecord->previousFramePointer = previousFramePointer;
        frameRecord->returnAddress = returnAddress;
        frameRecord->layout = QStringLiteral("[FP]=prevFP, [FP+4]=LR");
        return true;
    }

    const quint32 validBytes = qMin<quint32>(record.stackSize, static_cast<quint32>(record.stackData.size()));
    const quint64 stackEnd = static_cast<quint64>(record.stackStartAddr) + static_cast<quint64>(validBytes);
    const quint64 searchEnd = qMin<quint64>(stackEnd, static_cast<quint64>(framePointer) + 256u);
    for (quint64 addr64 = framePointer; addr64 + 8u <= searchEnd; addr64 += 4u)
    {
        const quint32 addr = static_cast<quint32>(addr64);
        if (!readStackWord(record, addr, &previousFramePointer) ||
            !readStackWord(record, addr + 4u, &returnAddress))
        {
            continue;
        }
        if (!isValidNextFramePointer(record, framePointer, previousFramePointer) ||
            !isLikelyReturnAddress(returnAddress))
        {
            continue;
        }

        frameRecord->recordAddress = addr;
        frameRecord->previousFramePointer = previousFramePointer;
        frameRecord->returnAddress = returnAddress;
        frameRecord->layout = QStringLiteral("FP窗口搜索 +0x%1").arg(addr - framePointer, 0, 16).toUpper();
        return true;
    }

    return false;
}

void MainWindow::addTraceAddress(QVector<TraceRow> *rows, QSet<quint32> *seen, const QString &source, quint32 address, bool isReturnAddress, const QString &note)
{
    if (address == 0u || address == 0xFFFFFFFFu)
    {
        return;
    }

    const SourceLocation loc = m_resolver.resolve(address, isReturnAddress);
    if (!loc.valid && !source.startsWith(QStringLiteral("PC")))
    {
        return;
    }

    if (seen->contains(loc.lookupAddress))
    {
        return;
    }
    seen->insert(loc.lookupAddress);

    TraceRow row;
    row.source = source;
    row.inputAddress = address;
    row.location = loc;
    row.note = note;
    rows->append(row);
}

int MainWindow::appendFramePointerTrace(const ExceptionRecord &record, QVector<TraceRow> *rows, QSet<quint32> *seen)
{
    if (rows == nullptr || seen == nullptr || record.r7 == 0u || record.r7 == 0xFFFFFFFFu)
    {
        return 0;
    }

    quint32 framePointer = record.r7;
    QSet<quint32> seenFramePointers;
    int appended = 0;
    const int maxDepth = 32;

    for (int depth = 0; depth < maxDepth; ++depth)
    {
        if ((framePointer & 0x3u) != 0u ||
            !isAddressInStackSnapshot(record, framePointer, 8u) ||
            seenFramePointers.contains(framePointer))
        {
            break;
        }
        seenFramePointers.insert(framePointer);

        FramePointerRecord frameRecord;
        if (!findFramePointerRecord(record, framePointer, &frameRecord))
        {
            break;
        }

        const int before = rows->size();
        addTraceAddress(rows,
                        seen,
                        QStringLiteral("FP[%1]").arg(hex32(framePointer)),
                        frameRecord.returnAddress,
                        true,
                        QStringLiteral("Frame Pointer链：%1，record=%2，prevFP=%3")
                            .arg(frameRecord.layout,
                                 hex32(frameRecord.recordAddress),
                                 hex32(frameRecord.previousFramePointer)));
        if (rows->size() > before)
        {
            ++appended;
            if (rows->last().location.displayFunction() == QStringLiteral("main"))
            {
                break;
            }
        }

        if (frameRecord.previousFramePointer == 0u ||
            frameRecord.previousFramePointer == 0xFFFFFFFFu)
        {
            break;
        }
        framePointer = frameRecord.previousFramePointer;
    }

    return appended;
}

void MainWindow::appendStackScanTrace(const ExceptionRecord &record, QVector<TraceRow> *rows, QSet<quint32> *seen)
{
    if (rows == nullptr || seen == nullptr)
    {
        return;
    }

    const QVector<quint32> words = record.stackWords();
    const int maxRows = 40;
    for (int i = 0; i < words.size() && rows->size() < maxRows; ++i)
    {
        const quint32 value = words.at(i);
        if (!isLikelyReturnAddress(value))
        {
            continue;
        }

        const quint32 stackAddr = record.stackStartAddr + static_cast<quint32>(i * 4);
        addTraceAddress(rows,
                        seen,
                        QStringLiteral("栈[%1]").arg(hex32(stackAddr)),
                        value,
                        true,
                        QStringLiteral("栈快照中疑似 Thumb 返回地址"));

        if (!rows->isEmpty() && rows->last().location.displayFunction() == QStringLiteral("main"))
        {
            break;
        }
    }
}

QVector<MainWindow::TraceRow> MainWindow::buildTrace(const ExceptionRecord &record)
{
    QVector<TraceRow> rows;
    QSet<quint32> seen;

    addTraceAddress(&rows, &seen, QStringLiteral("PC"), record.pc, false, QStringLiteral("异常点"));
    addTraceAddress(&rows, &seen, QStringLiteral("LR"), record.lr, true, QStringLiteral("异常点上一层调用者返回地址"));

    if (m_framePointerCheckBox != nullptr && m_framePointerCheckBox->isChecked())
    {
        if (record.r7 != 0u && record.r7 != 0xFFFFFFFFu)
        {
            appendFramePointerTrace(record, &rows, &seen);
        }
        return rows;
    }

    appendStackScanTrace(record, &rows, &seen);

    return rows;
}

void MainWindow::fillRegisterTable(const ExceptionRecord &record)
{
    const QVector<RegisterRow> rows = record.registerRows();
    m_registerTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i)
    {
        const RegisterRow &row = rows.at(i);
        m_registerTable->setItem(i, 0, item(row.group));
        m_registerTable->setItem(i, 1, item(row.name));
        m_registerTable->setItem(i, 2, item(hex32(row.value)));
        m_registerTable->setItem(i, 3, item(row.description));
    }
    m_registerTable->resizeColumnsToContents();
}

void MainWindow::fillFaultView(const ExceptionRecord &record)
{
    QStringList lines;
    lines << QStringLiteral("Fault 类型：%1").arg(ExceptionDumpParser::faultTypeName(record.faultType));
    lines << QStringLiteral("CFSR=%1 HFSR=%2 DFSR=%3 AFSR=%4")
                 .arg(hex32(record.cfsr), hex32(record.hfsr), hex32(record.dfsr), hex32(record.afsr));
    lines << QString();
    lines << ExceptionDumpParser::decodeFaultBits(record);

    const QStringList stackWarnings = ExceptionDumpParser::stackSanityWarnings(record);
    if (!stackWarnings.isEmpty())
    {
        lines << QString();
        lines << QStringLiteral("栈/现场一致性检查：");
        for (const QString &warning : stackWarnings)
        {
            lines << QStringLiteral("- %1").arg(warning);
        }
    }
    else
    {
        lines << QString();
        lines << QStringLiteral("栈/现场一致性检查：未发现明显异常。");
    }

    m_faultEdit->setPlainText(lines.join(QStringLiteral("\n")));
}

void MainWindow::fillTraceTable(const QVector<TraceRow> &rows)
{
    m_traceTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i)
    {
        const TraceRow &row = rows.at(i);
        m_traceTable->setItem(i, 0, item(QString::number(i)));
        m_traceTable->setItem(i, 1, item(row.source));
        m_traceTable->setItem(i, 2, item(QStringLiteral("%1 -> %2").arg(hex32(row.inputAddress), hex32(row.location.lookupAddress))));
        m_traceTable->setItem(i, 3, item(row.location.displayFunction()));
        m_traceTable->setItem(i, 4, item(row.location.displayFileLine()));
        m_traceTable->setItem(i, 5, item(row.note));
    }
    m_traceTable->resizeColumnsToContents();
}

void MainWindow::fillStackTable(const ExceptionRecord &record)
{
    const QVector<quint32> words = record.stackWords();
    const int rows = qMin(words.size(), 512);
    m_stackTable->setRowCount(rows);

    for (int i = 0; i < rows; ++i)
    {
        const quint32 value = words.at(i);
        const quint32 addr = record.stackStartAddr + static_cast<quint32>(i * 4);
        QString symbol;
        if (isUsefulStackWord(value) && m_resolver.isKnownCodeAddress(value, true))
        {
            const SourceLocation loc = m_resolver.resolve(value, true);
            symbol = QStringLiteral("%1  %2").arg(loc.displayFunction(), loc.displayFileLine());
        }
        m_stackTable->setItem(i, 0, item(QStringLiteral("+0x%1").arg(i * 4, 4, 16, QLatin1Char('0')).toUpper()));
        m_stackTable->setItem(i, 1, item(hex32(addr)));
        m_stackTable->setItem(i, 2, item(hex32(value)));
        m_stackTable->setItem(i, 3, item(symbol));
    }
    m_stackTable->resizeColumnsToContents();
}
