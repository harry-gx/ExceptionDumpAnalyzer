#include "can/can_transfer_widget.h"

#include "can/can_driver.h"
#include "can/can_worker.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
QTableWidgetItem *readOnlyItem(const QString &text)
{
    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}
}

CanTransferWidget::CanTransferWidget(QWidget *parent)
    : QWidget(parent)
    , m_worker(new CanWorker(this))
{
    buildUi();

    connect(m_openButton, &QPushButton::clicked, this, &CanTransferWidget::openDevice);
    connect(m_closeButton, &QPushButton::clicked, this, &CanTransferWidget::closeDevice);
    connect(m_sendButton, &QPushButton::clicked, this, &CanTransferWidget::sendFrame);
    connect(m_frameProtocolCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int) {
        const bool fd = m_frameProtocolCombo->currentData().toInt() != 0;
        if (fd)
        {
            m_txTypeCombo->setCurrentIndex(0);
        }
        updateState();
    });
    connect(m_driverCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CanTransferWidget::onDriverChanged);
    connect(m_clearLogButton, &QPushButton::clicked, this, &CanTransferWidget::clearLog);
    connect(m_clearBinaryButton, &QPushButton::clicked, this, &CanTransferWidget::clearReceivedBinary);
    connect(m_browseSaveButton, &QPushButton::clicked, this, &CanTransferWidget::browseSavePath);
    connect(m_saveButton, &QPushButton::clicked, this, &CanTransferWidget::saveBinary);
    connect(m_saveAnalyzeButton, &QPushButton::clicked, this, &CanTransferWidget::saveBinaryAndAnalyze);
    connect(m_captureFilterCheck, &QCheckBox::toggled, this, &CanTransferWidget::applyReceiveFilter);
    connect(m_captureIdEdit, &QLineEdit::textChanged, this, &CanTransferWidget::applyReceiveFilter);
    connect(m_captureFormatCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CanTransferWidget::applyReceiveFilter);
    connect(m_displayModeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int) {
        m_table->setRowCount(0);
        m_groupedRows.clear();
    });
    connect(m_worker, &CanWorker::deviceOpened, this, &CanTransferWidget::onDeviceOpened);
    connect(m_worker, &CanWorker::openFailed, this, &CanTransferWidget::onOpenFailed);
    connect(m_worker, &CanWorker::deviceClosed, this, &CanTransferWidget::onDeviceClosed);
    connect(m_worker, &CanWorker::frameReceived, this, &CanTransferWidget::onFrameReceived);

    m_savePathEdit->setText(defaultSavePath());
    applyReceiveFilter();
    updateState();
}

CanTransferWidget::~CanTransferWidget()
{
    closeDevice();
}

void CanTransferWidget::buildUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    QGroupBox *deviceBox = new QGroupBox(QStringLiteral("CAN设备"), this);
    QGridLayout *deviceGrid = new QGridLayout(deviceBox);

    m_driverCombo = new QComboBox(deviceBox);
    const QStringList drivers = CanDriverFactory::availableDrivers();
    for (const QString &driver : drivers)
    {
        m_driverCombo->addItem(driver, driver);
    }

    m_deviceTypeCombo = new QComboBox(deviceBox);
    m_deviceTypeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_deviceTypeCombo->setMinimumContentsLength(14);
    m_deviceTypeCombo->setMinimumWidth(145);
    m_deviceTypeCombo->view()->setMinimumWidth(145);
    refreshDeviceTypes();

    m_deviceIndexSpin = new QSpinBox(deviceBox);
    m_deviceIndexSpin->setRange(0, 15);

    m_channelCombo = new QComboBox(deviceBox);
    m_channelCombo->addItem(QStringLiteral("CAN1"), 0);
    m_channelCombo->addItem(QStringLiteral("CAN2"), 1);

    m_baudCombo = new QComboBox(deviceBox);
    for (int i = 0; i < CanBitrate::count(); ++i)
    {
        m_baudCombo->addItem(CanBitrate::label(i), i);
    }
    m_baudCombo->setCurrentIndex(3);

    m_modeCombo = new QComboBox(deviceBox);
    m_modeCombo->addItem(QStringLiteral("正常模式"), 0);
    m_modeCombo->addItem(QStringLiteral("只听模式"), 1);
    m_frameProtocolCombo = new QComboBox(deviceBox);
    m_frameProtocolCombo->addItem(QStringLiteral("普通CAN"), 0);
    m_frameProtocolCombo->addItem(QStringLiteral("CAN FD"), 1);
    m_frameProtocolCombo->addItem(QStringLiteral("CAN FD(加速)"), 2);

    m_openButton = new QPushButton(QStringLiteral("打开设备"), deviceBox);
    m_closeButton = new QPushButton(QStringLiteral("关闭设备"), deviceBox);
    m_statusLabel = new QLabel(QStringLiteral("未连接"), deviceBox);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    deviceGrid->addWidget(new QLabel(QStringLiteral("驱动"), deviceBox), 0, 0);
    deviceGrid->addWidget(m_driverCombo, 0, 1);
    deviceGrid->addWidget(new QLabel(QStringLiteral("设备类型"), deviceBox), 0, 2);
    deviceGrid->addWidget(m_deviceTypeCombo, 0, 3);
    deviceGrid->addWidget(new QLabel(QStringLiteral("设备索引"), deviceBox), 0, 4);
    deviceGrid->addWidget(m_deviceIndexSpin, 0, 5);
    deviceGrid->addWidget(new QLabel(QStringLiteral("通道"), deviceBox), 1, 0);
    deviceGrid->addWidget(m_channelCombo, 1, 1);
    deviceGrid->addWidget(new QLabel(QStringLiteral("波特率"), deviceBox), 1, 2);
    deviceGrid->addWidget(m_baudCombo, 1, 3);
    deviceGrid->addWidget(new QLabel(QStringLiteral("模式"), deviceBox), 1, 4);
    deviceGrid->addWidget(m_modeCombo, 1, 5);
    deviceGrid->addWidget(new QLabel(QStringLiteral("协议"), deviceBox), 2, 0);
    deviceGrid->addWidget(m_frameProtocolCombo, 2, 1);
    deviceGrid->addWidget(m_openButton, 0, 6);
    deviceGrid->addWidget(m_closeButton, 1, 6);
    deviceGrid->addWidget(m_statusLabel, 0, 7, 3, 1);
    deviceGrid->setColumnStretch(7, 1);

    QGroupBox *sendBox = new QGroupBox(QStringLiteral("CAN发送"), this);
    QGridLayout *sendGrid = new QGridLayout(sendBox);
    m_txIdEdit = new QLineEdit(QStringLiteral("511"), sendBox);
    m_txDataEdit = new QLineEdit(QStringLiteral("23 ff 00 00 00 00 00 00"), sendBox);
    m_txFormatCombo = new QComboBox(sendBox);
    m_txFormatCombo->addItem(QStringLiteral("标准帧"), 0);
    m_txFormatCombo->addItem(QStringLiteral("扩展帧"), 1);
    m_txTypeCombo = new QComboBox(sendBox);
    m_txTypeCombo->addItem(QStringLiteral("数据帧"), 0);
    m_txTypeCombo->addItem(QStringLiteral("远程帧"), 1);
    m_sendButton = new QPushButton(QStringLiteral("发送"), sendBox);

    sendGrid->addWidget(new QLabel(QStringLiteral("ID (HEX)"), sendBox), 0, 0);
    sendGrid->addWidget(m_txIdEdit, 0, 1);
    sendGrid->addWidget(new QLabel(QStringLiteral("帧格式"), sendBox), 0, 2);
    sendGrid->addWidget(m_txFormatCombo, 0, 3);
    sendGrid->addWidget(new QLabel(QStringLiteral("数据 (HEX)"), sendBox), 1, 0);
    sendGrid->addWidget(m_txDataEdit, 1, 1);
    sendGrid->addWidget(new QLabel(QStringLiteral("帧类型"), sendBox), 1, 2);
    sendGrid->addWidget(m_txTypeCombo, 1, 3);
    sendGrid->addWidget(m_sendButton, 0, 4, 2, 1);
    sendGrid->setColumnStretch(1, 1);

    QGroupBox *rxBox = new QGroupBox(QStringLiteral("CAN收发日志与异常数据保存"), this);
    QVBoxLayout *rxLayout = new QVBoxLayout(rxBox);
    QGridLayout *captureGrid = new QGridLayout;

    m_captureFilterCheck = new QCheckBox(QStringLiteral("只接收指定ID"), rxBox);
    m_captureIdEdit = new QLineEdit(QStringLiteral("666"), rxBox);
    m_captureIdEdit->setPlaceholderText(QStringLiteral("接收ID HEX，多个用空格/逗号分隔"));
    m_captureFormatCombo = new QComboBox(rxBox);
    m_captureFormatCombo->addItem(QStringLiteral("标准帧"), 0);
    m_captureFormatCombo->addItem(QStringLiteral("扩展帧"), 1);
    m_displayModeCombo = new QComboBox(rxBox);
    m_displayModeCombo->addItem(QStringLiteral("顺序模式"), 0);
    m_displayModeCombo->addItem(QStringLiteral("分组模式"), 1);
    m_savePathEdit = new QLineEdit(rxBox);
    m_browseSaveButton = new QPushButton(QStringLiteral("浏览"), rxBox);
    m_clearLogButton = new QPushButton(QStringLiteral("清空日志"), rxBox);
    m_clearBinaryButton = new QPushButton(QStringLiteral("清空已接收数据"), rxBox);
    m_saveButton = new QPushButton(QStringLiteral("保存二进制"), rxBox);
    m_saveAnalyzeButton = new QPushButton(QStringLiteral("保存并解析"), rxBox);
    m_statsLabel = new QLabel(QStringLiteral("TX: 0  RX: 0  已接收: 0 bytes"), rxBox);
    m_statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    captureGrid->addWidget(m_captureFilterCheck, 0, 0);
    captureGrid->addWidget(m_captureIdEdit, 0, 1);
    captureGrid->addWidget(m_captureFormatCombo, 0, 2);
    captureGrid->addWidget(new QLabel(QStringLiteral("显示模式"), rxBox), 0, 3);
    captureGrid->addWidget(m_displayModeCombo, 0, 4);
    captureGrid->addWidget(m_clearLogButton, 0, 5);
    captureGrid->addWidget(m_clearBinaryButton, 0, 6);
    captureGrid->addWidget(m_saveButton, 0, 7);
    captureGrid->addWidget(m_saveAnalyzeButton, 0, 8);
    captureGrid->addWidget(new QLabel(QStringLiteral("保存路径"), rxBox), 1, 0);
    captureGrid->addWidget(m_savePathEdit, 1, 1, 1, 6);
    captureGrid->addWidget(m_browseSaveButton, 1, 7);
    captureGrid->addWidget(m_statsLabel, 1, 8);
    captureGrid->setColumnStretch(1, 1);

    m_table = new QTableWidget(rxBox);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(QStringList()
                                       << QStringLiteral("时间(ms)")
                                       << QStringLiteral("方向")
                                       << QStringLiteral("帧ID")
                                       << QStringLiteral("类型")
                                       << QStringLiteral("DLC")
                                       << QStringLiteral("数据"));
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setColumnWidth(0, 90);
    m_table->setColumnWidth(1, 60);
    m_table->setColumnWidth(2, 90);
    m_table->setColumnWidth(3, 100);
    m_table->setColumnWidth(4, 50);

    rxLayout->addLayout(captureGrid);
    rxLayout->addWidget(m_table, 1);

    mainLayout->addWidget(deviceBox);
    mainLayout->addWidget(sendBox);
    mainLayout->addWidget(rxBox, 1);
}

void CanTransferWidget::updateState()
{
    const bool opened = m_worker->isDeviceOpen();
    const bool supportsCanFd = m_driverCombo->currentData().toString().contains(QStringLiteral("USBCANFD"));
    const bool frameFd = m_frameProtocolCombo->currentData().toInt() != 0;
    m_openButton->setEnabled(!opened);
    m_closeButton->setEnabled(opened);
    m_sendButton->setEnabled(opened);
    m_frameProtocolCombo->setEnabled(!opened && supportsCanFd);
    m_txTypeCombo->setEnabled(!frameFd);
    m_driverCombo->setEnabled(!opened);
    m_deviceTypeCombo->setEnabled(!opened);
    m_deviceIndexSpin->setEnabled(!opened);
    m_channelCombo->setEnabled(!opened);
    m_baudCombo->setEnabled(!opened);
    m_modeCombo->setEnabled(!opened);
    m_statsLabel->setText(QStringLiteral("TX: %1  RX: %2  已接收: %3 bytes")
                              .arg(m_txCount)
                              .arg(m_rxCount)
                              .arg(m_receivedBinary.size()));
}

void CanTransferWidget::refreshDeviceTypes()
{
    if (!m_driverCombo || !m_deviceTypeCombo)
    {
        return;
    }

    const QString driverName = m_driverCombo->currentData().toString();
    const QVector<CanDeviceTypeInfo> types = CanDriverFactory::deviceTypes(driverName);

    m_deviceTypeCombo->blockSignals(true);
    m_deviceTypeCombo->clear();
    for (const CanDeviceTypeInfo &type : types)
    {
        m_deviceTypeCombo->addItem(type.name, type.value);
    }
    m_deviceTypeCombo->blockSignals(false);

    m_deviceTypeCombo->setMinimumWidth(qMax(145, m_deviceTypeCombo->sizeHint().width()));
    m_deviceTypeCombo->view()->setMinimumWidth(m_deviceTypeCombo->minimumWidth());
}

void CanTransferWidget::onDriverChanged(int index)
{
    Q_UNUSED(index);
    refreshDeviceTypes();
    if (!m_frameProtocolCombo || !m_driverCombo)
    {
        return;
    }

    const QString driverName = m_driverCombo->currentData().toString();
    m_frameProtocolCombo->setCurrentIndex(driverName.contains(QStringLiteral("USBCANFD")) ? 2 : 0);
    updateState();
}

void CanTransferWidget::openDevice()
{
    CanDriverConfig config;
    config.deviceType = m_deviceTypeCombo->currentData().toInt();
    config.deviceIndex = m_deviceIndexSpin->value();
    config.channelIndex = m_channelCombo->currentData().toInt();
    config.baudIndex = m_baudCombo->currentData().toInt();
    config.mode = m_modeCombo->currentData().toInt();
    config.frameMode = m_frameProtocolCombo->isEnabled() ? m_frameProtocolCombo->currentData().toInt() : 0;
    applyReceiveFilter();
    m_worker->openDevice(m_driverCombo->currentData().toString(), config);
}

void CanTransferWidget::closeDevice()
{
    if (m_worker)
    {
        m_worker->closeDevice();
    }
}

void CanTransferWidget::sendFrame()
{
    const bool ext = m_txFormatCombo->currentData().toInt() != 0;
    const bool rtr = m_txTypeCombo->currentData().toInt() != 0;
    const int fdMode = m_worker->frameMode();
    const bool fd = fdMode != 0;

    quint32 id = 0;
    if (!parseCanId(m_txIdEdit->text(), ext, &id))
    {
        QMessageBox::warning(this, QStringLiteral("CAN发送"), QStringLiteral("CAN ID格式不正确。"));
        return;
    }

    bool dataOk = false;
    const QByteArray data = parseHexBytes(m_txDataEdit->text(), &dataOk);
    if (!rtr && (!dataOk || data.isEmpty()))
    {
        QMessageBox::warning(this, QStringLiteral("CAN发送"), QStringLiteral("数据帧至少需要1个HEX字节。"));
        return;
    }
    if (fd && rtr)
    {
        QMessageBox::warning(this, QStringLiteral("CAN发送"), QStringLiteral("CAN FD 不支持远程帧，请选择数据帧。"));
        return;
    }

    const int maxDataBytes = fd ? 64 : 8;
    if (data.size() > maxDataBytes)
    {
        QMessageBox::warning(this, QStringLiteral("CAN发送"), QStringLiteral("当前发送模式单帧数据最多%1字节。").arg(maxDataBytes));
        return;
    }

    m_worker->transmitFrame(id, data, ext, rtr);
}

void CanTransferWidget::clearLog()
{
    m_table->setRowCount(0);
    m_groupedRows.clear();
    m_txCount = 0;
    m_rxCount = 0;
    updateState();
}

void CanTransferWidget::clearReceivedBinary()
{
    m_receivedBinary.clear();
    if (m_worker)
    {
        m_worker->clearRxBuffer();
    }
    updateState();
}

void CanTransferWidget::browseSavePath()
{
    const QString path = QFileDialog::getSaveFileName(this,
                                                      QStringLiteral("保存异常现场二进制文件"),
                                                      m_savePathEdit->text().isEmpty() ? defaultSavePath() : m_savePathEdit->text(),
                                                      QStringLiteral("Binary (*.bin);;All files (*.*)"));
    if (!path.isEmpty())
    {
        m_savePathEdit->setText(QDir::toNativeSeparators(path));
    }
}

void CanTransferWidget::saveBinary()
{
    const QString path = m_savePathEdit->text().trimmed();
    if (writeBinaryToFile(path))
    {
        emit binarySaved(path);
    }
}

void CanTransferWidget::saveBinaryAndAnalyze()
{
    const QString path = m_savePathEdit->text().trimmed();
    if (writeBinaryToFile(path))
    {
        emit binarySavedAndAnalyze(path);
    }
}

void CanTransferWidget::onDeviceOpened(const QString &message)
{
    m_statusLabel->setText(message);
    updateState();
}

void CanTransferWidget::onOpenFailed(const QString &reason)
{
    m_statusLabel->setText(reason);
    QMessageBox::warning(this, QStringLiteral("CAN设备"), reason);
    updateState();
}

void CanTransferWidget::onDeviceClosed()
{
    m_statusLabel->setText(QStringLiteral("未连接"));
    updateState();
}

void CanTransferWidget::onFrameReceived(const QString &timeMs, const QString &dir, const QString &idHex,
                                        const QString &frameType, const QString &dlc, const QString &dataHex,
                                        quint32 id, bool ext, bool rtr, const QByteArray &rawData)
{
    if (dir == QStringLiteral("TX"))
    {
        ++m_txCount;
    }
    else if (dir == QStringLiteral("RX"))
    {
        if (!shouldAcceptRxFrame(id, ext))
        {
            return;
        }
        ++m_rxCount;
        if (!rtr && !rawData.isEmpty())
        {
            m_receivedBinary.append(rawData);
        }
    }

    appendLogRow(timeMs, dir, idHex, frameType, dlc, dataHex, id, ext);
    updateState();
}

void CanTransferWidget::appendLogRow(const QString &timeMs, const QString &dir, const QString &idHex,
                                     const QString &frameType, const QString &dlc, const QString &dataHex,
                                     quint32 id, bool ext)
{
    const bool groupRx = (dir == QStringLiteral("RX")) &&
                         m_displayModeCombo &&
                         m_displayModeCombo->currentData().toInt() == 1;
    const QString groupKey = groupRx ? rxGroupKey(id, ext) : QString();

    if (groupRx && m_groupedRows.contains(groupKey))
    {
        setLogRow(m_groupedRows.value(groupKey), timeMs, dir, idHex, frameType, dlc, dataHex, groupKey);
        return;
    }

    trimLogRows();
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    setLogRow(row, timeMs, dir, idHex, frameType, dlc, dataHex, groupKey);
    if (groupRx)
    {
        m_groupedRows.insert(groupKey, row);
    }
    m_table->scrollToBottom();
}

void CanTransferWidget::trimLogRows()
{
    while (m_table->rowCount() >= kMaxRows)
    {
        m_table->removeRow(0);
        rebuildGroupedRows();
    }
}

void CanTransferWidget::rebuildGroupedRows()
{
    m_groupedRows.clear();
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        QTableWidgetItem *idItem = m_table->item(row, 2);
        if (!idItem)
        {
            continue;
        }

        const QString groupKey = idItem->data(Qt::UserRole).toString();
        if (!groupKey.isEmpty())
        {
            m_groupedRows.insert(groupKey, row);
        }
    }
}

void CanTransferWidget::setLogRow(int row, const QString &timeMs, const QString &dir, const QString &idHex,
                                  const QString &frameType, const QString &dlc, const QString &dataHex,
                                  const QString &groupKey)
{
    m_table->setItem(row, 0, readOnlyItem(timeMs));
    m_table->setItem(row, 1, readOnlyItem(dir));
    QTableWidgetItem *idItem = readOnlyItem(idHex);
    idItem->setData(Qt::UserRole, groupKey);
    m_table->setItem(row, 2, idItem);
    m_table->setItem(row, 3, readOnlyItem(frameType));
    m_table->setItem(row, 4, readOnlyItem(dlc));
    m_table->setItem(row, 5, readOnlyItem(dataHex));
}

QString CanTransferWidget::rxGroupKey(quint32 id, bool ext) const
{
    return QStringLiteral("%1:%2").arg(ext ? 1 : 0).arg(id, ext ? 8 : 3, 16, QLatin1Char('0')).toUpper();
}

bool CanTransferWidget::parseCanId(const QString &text, bool ext, quint32 *idOut) const
{
    QString valueText = text.trimmed();
    if (valueText.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
    {
        valueText = valueText.mid(2);
    }

    bool ok = false;
    const quint32 id = valueText.toUInt(&ok, 16);
    if (!ok || (!ext && id > 0x7FFu) || (ext && id > 0x1FFFFFFFu))
    {
        return false;
    }
    if (idOut)
    {
        *idOut = id;
    }
    return true;
}

QVector<quint32> CanTransferWidget::parseCanIds(const QString &text, bool ext, bool *okOut) const
{
    QVector<quint32> ids;
    bool allOk = true;
    QString normalized = text;
    normalized.replace(QLatin1Char(','), QLatin1Char(' '));
    normalized.replace(QLatin1Char(';'), QLatin1Char(' '));

    const QStringList parts = normalized.simplified().split(QLatin1Char(' '), QString::SkipEmptyParts);
    for (const QString &part : parts)
    {
        quint32 id = 0;
        if (!parseCanId(part, ext, &id))
        {
            allOk = false;
            break;
        }
        if (!ids.contains(id))
        {
            ids.append(id);
        }
    }

    if (ids.isEmpty())
    {
        allOk = false;
    }

    if (okOut)
    {
        *okOut = allOk;
    }
    return allOk ? ids : QVector<quint32>();
}

QByteArray CanTransferWidget::parseHexBytes(const QString &text, bool *okOut) const
{
    QByteArray out;
    bool allOk = true;
    const QString simplified = text.simplified();
    if (!simplified.isEmpty())
    {
        const QStringList parts = simplified.split(QLatin1Char(' '), QString::SkipEmptyParts);
        for (const QString &part : parts)
        {
            bool ok = false;
            const int value = part.toInt(&ok, 16);
            if (!ok || value < 0 || value > 255)
            {
                allOk = false;
                break;
            }
            out.append(char(value));
        }
    }
    if (okOut)
    {
        *okOut = allOk;
    }
    return out;
}

void CanTransferWidget::applyReceiveFilter()
{
    if (!m_worker || !m_captureFilterCheck || !m_captureIdEdit || !m_captureFormatCombo)
    {
        return;
    }

    if (!m_captureFilterCheck->isChecked())
    {
        m_worker->setReceiveFilter(false, QVector<quint32>(), false);
        return;
    }

    bool ok = false;
    const bool filterExt = m_captureFormatCombo->currentData().toInt() != 0;
    const QVector<quint32> filterIds = parseCanIds(m_captureIdEdit->text(), filterExt, &ok);
    m_worker->setReceiveFilter(true, ok ? filterIds : QVector<quint32>(), filterExt);
}

bool CanTransferWidget::shouldAcceptRxFrame(quint32 id, bool ext) const
{
    if (!m_captureFilterCheck->isChecked())
    {
        return true;
    }

    bool ok = false;
    const bool filterExt = m_captureFormatCombo->currentData().toInt() != 0;
    const QVector<quint32> filterIds = parseCanIds(m_captureIdEdit->text(), filterExt, &ok);
    if (!ok)
    {
        return false;
    }
    return ext == filterExt && filterIds.contains(id);
}

bool CanTransferWidget::writeBinaryToFile(const QString &path)
{
    if (path.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("保存二进制"), QStringLiteral("请先设置保存路径。"));
        return false;
    }
    if (m_receivedBinary.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("保存二进制"), QStringLiteral("当前还没有收到可保存的数据帧。"));
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, QStringLiteral("保存二进制"), QStringLiteral("无法写入文件：%1").arg(file.errorString()));
        return false;
    }
    file.write(m_receivedBinary);
    file.close();

    QMessageBox::information(this,
                             QStringLiteral("保存二进制"),
                             QStringLiteral("已保存 %1 bytes 到：\n%2").arg(m_receivedBinary.size()).arg(QDir::toNativeSeparators(path)));
    return true;
}

QString CanTransferWidget::defaultSavePath() const
{
    QDir dir(QCoreApplication::applicationDirPath());
    return QDir::toNativeSeparators(dir.absoluteFilePath(QStringLiteral("test.bin")));
}
