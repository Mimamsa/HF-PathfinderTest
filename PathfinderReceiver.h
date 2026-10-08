#ifndef PATHFINDERRECEIVER_H
#define PATHFINDERRECEIVER_H

#include <QWidget>
#include <QSerialPort>
#include <QFile>
#include <QVector>
class QComboBox;
class QPushButton;
class QLabel;
class QPlainTextEdit;
class QTableWidget;
class QSpinBox;

// Receives the UART stream without issuing radio configuration commands.
class PathfinderReceiver : public QWidget
{
    Q_OBJECT
public:
    explicit PathfinderReceiver(QWidget *parent = nullptr);
private:
    void refreshPorts();
    void toggleConnection();
    void receiveData();
    void disconnectPort();
    void updateControls();
    void toggleRecording();
    void stopRecording();
    void processFrames();
    void updateDecodedValues(const QByteArray &packet);
    void captureBaseline();
    static quint16 crc16Xmodem(const QByteArray &data);
    QSerialPort serial;
    QFile recording;
    QComboBox *ports;
    QComboBox *baud;
    QComboBox *parity;
    QPushButton *refresh;
    QPushButton *connectButton;
    QPushButton *recordButton;
    QLabel *status;
    QLabel *countLabel;
    QLabel *packetLabel;
    QLabel *but1Label;
    QPlainTextEdit *output;
    QTableWidget *decodedTable;
    QPushButton *baselineButton;
    QSpinBox *analogThreshold;
    QByteArray receiveBuffer;
    QVector<qint64> currentValues;
    QVector<qint64> baselineValues;
    bool baselineCaptured = false;
    quint64 receivedBytes = 0;
    quint64 validPackets = 0;
    quint64 rejectedBytes = 0;
};
#endif
