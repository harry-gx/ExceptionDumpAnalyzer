#ifndef CAN_TRANSFER_WIDGET_H
#define CAN_TRANSFER_WIDGET_H

#include <QByteArray>
#include <QHash>
#include <QWidget>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class CanWorker;

class CanTransferWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CanTransferWidget(QWidget *parent = nullptr);
    ~CanTransferWidget() override;

signals:
    void binarySaved(const QString &path);
    void binarySavedAndAnalyze(const QString &path);

private slots:
    void onDriverChanged(int index);
    void openDevice();
    void closeDevice();
    void sendFrame();
    void clearLog();
    void clearReceivedBinary();
    void browseSavePath();
    void saveBinary();
    void saveBinaryAndAnalyze();
    void onDeviceOpened(const QString &message);
    void onOpenFailed(const QString &reason);
    void onDeviceClosed();
    void onFrameReceived(const QString &timeMs, const QString &dir, const QString &idHex,
                         const QString &frameType, const QString &dlc, const QString &dataHex,
                         quint32 id, bool ext, bool rtr, const QByteArray &rawData);

private:
    void buildUi();
    void refreshDeviceTypes();
    void updateState();
    void appendLogRow(const QString &timeMs, const QString &dir, const QString &idHex,
                      const QString &frameType, const QString &dlc, const QString &dataHex,
                      quint32 id, bool ext);
    void trimLogRows();
    void rebuildGroupedRows();
    void setLogRow(int row, const QString &timeMs, const QString &dir, const QString &idHex,
                   const QString &frameType, const QString &dlc, const QString &dataHex,
                   const QString &groupKey);
    QString rxGroupKey(quint32 id, bool ext) const;
    bool parseCanId(const QString &text, bool ext, quint32 *idOut) const;
    QVector<quint32> parseCanIds(const QString &text, bool ext, bool *okOut) const;
    QByteArray parseHexBytes(const QString &text, bool *okOut) const;
    void applyReceiveFilter();
    bool shouldAcceptRxFrame(quint32 id, bool ext) const;
    bool writeBinaryToFile(const QString &path);
    QString defaultSavePath() const;

    CanWorker *m_worker = nullptr;
    QComboBox *m_driverCombo = nullptr;
    QComboBox *m_deviceTypeCombo = nullptr;
    QSpinBox *m_deviceIndexSpin = nullptr;
    QComboBox *m_channelCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_frameProtocolCombo = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QLineEdit *m_txIdEdit = nullptr;
    QLineEdit *m_txDataEdit = nullptr;
    QComboBox *m_txFormatCombo = nullptr;
    QComboBox *m_txTypeCombo = nullptr;
    QPushButton *m_sendButton = nullptr;

    QCheckBox *m_captureFilterCheck = nullptr;
    QLineEdit *m_captureIdEdit = nullptr;
    QComboBox *m_captureFormatCombo = nullptr;
    QComboBox *m_displayModeCombo = nullptr;
    QLineEdit *m_savePathEdit = nullptr;
    QPushButton *m_browseSaveButton = nullptr;
    QPushButton *m_clearLogButton = nullptr;
    QPushButton *m_clearBinaryButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_saveAnalyzeButton = nullptr;
    QLabel *m_statsLabel = nullptr;
    QTableWidget *m_table = nullptr;

    QByteArray m_receivedBinary;
    QHash<QString, int> m_groupedRows;
    int m_txCount = 0;
    int m_rxCount = 0;
    static const int kMaxRows = 2000;
};

#endif
