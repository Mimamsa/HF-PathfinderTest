#include "PathfinderReceiver.h"
#include <QSerialPortInfo>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QBrush>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>

PathfinderReceiver::PathfinderReceiver(QWidget *parent) : QWidget(parent)
{
    serial.setParent(this);
    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(tr("HANGFA Pathfinder / WTM-1-WU — Serial receiver"));
    layout->addWidget(title);
    auto *row = new QHBoxLayout;
    ports = new QComboBox;
    ports->setObjectName("pathfinderPort");
    ports->setEditable(true);
    ports->setMinimumWidth(260);
    baud = new QComboBox;
    baud->setObjectName("pathfinderBaud");
    for (int rate : {4800, 9600, 19200, 38400, 57600, 115200})
        baud->addItem(QString::number(rate), rate);
    baud->setCurrentText("115200");
    parity = new QComboBox;
    parity->setObjectName("pathfinderParity");
    parity->addItem(tr("None (8N1)"), QSerialPort::NoParity);
    parity->addItem(tr("Even (8E1)"), QSerialPort::EvenParity);
    parity->addItem(tr("Odd (8O1)"), QSerialPort::OddParity);
    refresh = new QPushButton(tr("Refresh ports"));
    connectButton = new QPushButton(tr("Connect"));
    connectButton->setObjectName("pathfinderConnect");
    row->addWidget(new QLabel(tr("Port")));
    row->addWidget(ports, 1);
    row->addWidget(new QLabel(tr("Baud")));
    row->addWidget(baud);
    row->addWidget(parity);
    row->addWidget(refresh);
    row->addWidget(connectButton);
    layout->addLayout(row);
    auto *help = new QLabel(tr("WTM-1-WU USB receiver: 115200 baud, 8N1. The serial port is opened receive-only; no radio settings are sent.\n"
                              "Mapping is experimental: capture a baseline, then move exactly one control. Changed candidates turn green."));
    help->setWordWrap(true);
    layout->addWidget(help);

    decodedTable = new QTableWidget(27, 5);
    decodedTable->setObjectName("pathfinderDecoded");
    decodedTable->setHorizontalHeaderLabels({tr("Signal"), tr("Current"), tr("Baseline"), tr("Delta / changed"), tr("Proposed source")});
    decodedTable->verticalHeader()->setVisible(false);
    decodedTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    decodedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    const QStringList names = {
        "JOY1 X", "JOY1 Y", "JOY1 Z", "JOY2 X", "JOY2 Y", "JOY3 X", "JOY3 Y",
        "POT A", "POT B", "Ban A", "Ban B", "ENC A", "ENC B",
        "JOY1 D", "JOY2 D", "JOY3 D", "TOG1", "TOG2", "TOG3", "TOG4", "TOG5",
        "BUT1", "BUT2", "BUT3", "BUT4", "BUT5", "BUT6"
    };
    QStringList sources;
    for (int offset = 5; offset <= 21; offset += 2)
        sources << tr("bytes %1-%2, int16 LE (CONFIRMED)").arg(offset).arg(offset + 1);
    sources << tr("byte 23 low nibble (CONFIRMED)") << tr("byte 23 high nibble (CONFIRMED)")
            << tr("byte 24, int8 (CONFIRMED)") << tr("byte 25, int8 (CONFIRMED)");
    sources << tr("byte 26 bit 6 (CONFIRMED)")
            << tr("byte 27 bit 7 (CONFIRMED)")
            << tr("byte 27 bit 6 (CONFIRMED)");
    for (int bit = 4; bit >= 0; --bit) sources << tr("byte 26 bit %1 (CONFIRMED)").arg(bit);
    sources << tr("byte 27 bit 5 (CONFIRMED)");
    for (int bit = 4; bit >= 0; --bit) sources << tr("byte 27 bit %1 (CONFIRMED)").arg(bit);
    for (int rowIndex = 0; rowIndex < names.size(); ++rowIndex) {
        decodedTable->setItem(rowIndex, 0, new QTableWidgetItem(names.at(rowIndex)));
        decodedTable->setItem(rowIndex, 1, new QTableWidgetItem(QStringLiteral("—")));
        decodedTable->setItem(rowIndex, 2, new QTableWidgetItem(QStringLiteral("—")));
        decodedTable->setItem(rowIndex, 3, new QTableWidgetItem(QStringLiteral("—")));
        decodedTable->setItem(rowIndex, 4, new QTableWidgetItem(sources.at(rowIndex)));
    }
    decodedTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    decodedTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    decodedTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    decodedTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    decodedTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    decodedTable->setMinimumHeight(300);
    layout->addWidget(decodedTable, 2);

    output = new QPlainTextEdit;
    output->setObjectName("pathfinderOutput");
    output->setReadOnly(true);
    output->document()->setMaximumBlockCount(2000);
    layout->addWidget(output, 1);
    auto *actions = new QHBoxLayout;
    auto *clear = new QPushButton(tr("Clear HEX log"));
    baselineButton = new QPushButton(tr("Capture baseline"));
    baselineButton->setObjectName("pathfinderBaseline");
    analogThreshold = new QSpinBox;
    analogThreshold->setObjectName("pathfinderThreshold");
    analogThreshold->setRange(0, 32767);
    analogThreshold->setValue(10);
    analogThreshold->setToolTip(tr("Minimum int16 change highlighted for joystick and potentiometer rows"));
    recordButton = new QPushButton(tr("Record raw data..."));
    countLabel = new QLabel(tr("Received: 0 bytes"));
    actions->addWidget(baselineButton);
    actions->addWidget(new QLabel(tr("Analog threshold")));
    actions->addWidget(analogThreshold);
    actions->addWidget(clear);
    actions->addWidget(recordButton);
    packetLabel = new QLabel(tr("Packets: 0 valid, 0 rejected bytes"));
    packetLabel->setObjectName("pathfinderPackets");
    but1Label = new QLabel(tr("BUT1: released"));
    but1Label->setObjectName("pathfinderBut1");
    actions->addWidget(countLabel);
    actions->addWidget(packetLabel);
    actions->addWidget(but1Label);
    actions->addStretch();
    layout->addLayout(actions);
    status = new QLabel(tr("Select the receiver port, then connect. WTM-1-WU default: 115200 baud, 8N1, no flow control."));
    status->setObjectName("pathfinderStatus");
    status->setWordWrap(true);
    layout->addWidget(status);
    connect(refresh, &QPushButton::clicked, this, &PathfinderReceiver::refreshPorts);
    connect(connectButton, &QPushButton::clicked, this, &PathfinderReceiver::toggleConnection);
    connect(clear, &QPushButton::clicked, output, &QPlainTextEdit::clear);
    connect(baselineButton, &QPushButton::clicked, this, &PathfinderReceiver::captureBaseline);
    connect(analogThreshold, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
        if (baselineCaptured && currentValues.size() == 27) updateDecodedValues(QByteArray());
    });
    connect(recordButton, &QPushButton::clicked, this, &PathfinderReceiver::toggleRecording);
    connect(ports, &QComboBox::currentTextChanged, this, [this] { updateControls(); });
    connect(&serial, &QSerialPort::readyRead, this, &PathfinderReceiver::receiveData);
    // Queue errors so disconnect does not re-enter QSerialPort's open/read operation.
    connect(&serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (!serial.isOpen() || error == QSerialPort::NoError || error == QSerialPort::TimeoutError
            || error == QSerialPort::UnsupportedOperationError)
            return;
        const QString message = serial.errorString();
        disconnectPort();
        status->setText(tr("Serial error: %1").arg(message));
    }, Qt::QueuedConnection);
    refreshPorts();
    updateControls();
}

void PathfinderReceiver::refreshPorts()
{
    if (serial.isOpen()) return;
    const QString previous = ports->currentText();
    ports->clear();
    for (const auto &info : QSerialPortInfo::availablePorts())
        ports->addItem(info.systemLocation());
    if (!previous.isEmpty()) ports->setCurrentText(previous);
    else if (ports->findText("/dev/ttyUSB0") >= 0) ports->setCurrentText("/dev/ttyUSB0");
    updateControls();
}

void PathfinderReceiver::updateControls()
{
    const bool connected = serial.isOpen();
    ports->setEnabled(!connected);
    baud->setEnabled(!connected);
    parity->setEnabled(!connected);
    refresh->setEnabled(!connected);
    connectButton->setText(connected ? tr("Disconnect") : tr("Connect"));
    connectButton->setEnabled(connected || !ports->currentText().trimmed().isEmpty());
    recordButton->setEnabled(connected);
}

void PathfinderReceiver::toggleConnection()
{
    if (serial.isOpen()) {
        disconnectPort();
        status->setText(tr("Disconnected."));
        return;
    }
    serial.setPortName(ports->currentText().trimmed());
    if (!serial.setBaudRate(baud->currentData().toInt())
        || !serial.setDataBits(QSerialPort::Data8)
        || !serial.setParity(static_cast<QSerialPort::Parity>(parity->currentData().toInt()))
        || !serial.setStopBits(QSerialPort::OneStop)
        || !serial.setFlowControl(QSerialPort::NoFlowControl)
        || !serial.open(QIODevice::ReadOnly)) {
        status->setText(tr("Cannot open %1: %2").arg(serial.portName(), serial.errorString()));
        updateControls();
        return;
    }
    // WTM-1-WU reception was verified to stop with RTS asserted and resume
    // with RTS deasserted. No UART command bytes are transmitted here.
    const bool rtsConfigured = serial.setRequestToSend(false);
    const QString rtsError = serial.errorString();
    if (!rtsConfigured && serial.error() != QSerialPort::UnsupportedOperationError) {
        serial.close();
        status->setText(tr("Cannot configure RTS on %1: %2").arg(serial.portName(), rtsError));
        updateControls();
        return;
    }
    receivedBytes = 0;
    validPackets = 0;
    rejectedBytes = 0;
    receiveBuffer.clear();
    baselineCaptured = false;
    baselineValues.clear();
    currentValues.clear();
    for (int rowIndex = 0; rowIndex < decodedTable->rowCount(); ++rowIndex) {
        decodedTable->item(rowIndex, 1)->setText(QStringLiteral("—"));
        decodedTable->item(rowIndex, 2)->setText(QStringLiteral("—"));
        decodedTable->item(rowIndex, 3)->setText(QStringLiteral("—"));
        for (int column = 0; column < decodedTable->columnCount(); ++column)
            decodedTable->item(rowIndex, column)->setBackground(QBrush());
    }
    countLabel->setText(tr("Received: 0 bytes"));
    packetLabel->setText(tr("Packets: 0 valid, 0 rejected bytes"));
    but1Label->setText(tr("BUT1: released"));
    output->appendPlainText(tr("--- Connected: %1, %2 baud, %3 ---")
                            .arg(serial.portName()).arg(serial.baudRate()).arg(parity->currentText()));
    status->setText(rtsConfigured
        ? tr("Connected. Waiting for remote data; press a button on the Pathfinder.")
        : tr("Connected, but RTS control is unavailable: %1").arg(rtsError));
    updateControls();
}

void PathfinderReceiver::disconnectPort()
{
    if (serial.isOpen()) {
        receiveData();
        serial.close();
    }
    stopRecording();
    updateControls();
}

void PathfinderReceiver::receiveData()
{
    const QByteArray data = serial.readAll();
    if (data.isEmpty()) return;
    receivedBytes += static_cast<quint64>(data.size());
    countLabel->setText(tr("Received: %1 bytes").arg(receivedBytes));
    receiveBuffer.append(data);
    processFrames();
    if (recording.isOpen() && (recording.write(data) != data.size() || !recording.flush())) {
        const QString error = recording.errorString();
        stopRecording();
        status->setText(tr("Recording failed: %1").arg(error));
    }
}


quint16 PathfinderReceiver::crc16Xmodem(const QByteArray &data)
{
    quint16 crc = 0;
    for (const char byte : data) {
        crc ^= static_cast<quint16>(static_cast<quint8>(byte)) << 8;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000) ? static_cast<quint16>((crc << 1) ^ 0x1021) : static_cast<quint16>(crc << 1);
    }
    return crc;
}

void PathfinderReceiver::processFrames()
{
    static const QByteArray header = QByteArray::fromHex("aa0f012f17");
    constexpr int packetSize = 31;
    while (true) {
        const int offset = receiveBuffer.indexOf(header);
        if (offset < 0) {
            const int keep = qMin(receiveBuffer.size(), header.size() - 1);
            rejectedBytes += static_cast<quint64>(receiveBuffer.size() - keep);
            receiveBuffer = receiveBuffer.right(keep);
            break;
        }
        if (offset > 0) { rejectedBytes += static_cast<quint64>(offset); receiveBuffer.remove(0, offset); }
        if (receiveBuffer.size() < packetSize) break;
        const QByteArray packet = receiveBuffer.left(packetSize);
        const quint16 expected = static_cast<quint8>(packet.at(28)) | (static_cast<quint16>(static_cast<quint8>(packet.at(29))) << 8);
        if (static_cast<quint8>(packet.at(30)) != 0x0d || crc16Xmodem(packet.left(28)) != expected) {
            ++rejectedBytes; receiveBuffer.remove(0, 1); continue;
        }
        receiveBuffer.remove(0, packetSize); ++validPackets;
        const bool pressed = (static_cast<quint8>(packet.at(27)) & 0x20) != 0;
        but1Label->setText(pressed ? tr("BUT1: PRESSED") : tr("BUT1: released"));
        updateDecodedValues(packet);
        output->appendPlainText(QStringLiteral("%1  %2  BUT1=%3").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), QString::fromLatin1(packet.toHex(' ').toUpper()), pressed ? QStringLiteral("PRESSED") : QStringLiteral("released")));
    }
    packetLabel->setText(tr("Packets: %1 valid, %2 rejected bytes").arg(validPackets).arg(rejectedBytes));
}

void PathfinderReceiver::updateDecodedValues(const QByteArray &packet)
{
    if (!packet.isEmpty()) {
        currentValues.clear();
        auto int16Le = [&packet](int offset) {
            const quint16 raw = static_cast<quint8>(packet.at(offset))
                              | (static_cast<quint16>(static_cast<quint8>(packet.at(offset + 1))) << 8);
            return static_cast<qint64>(static_cast<qint16>(raw));
        };
        for (int offset = 5; offset <= 21; offset += 2) currentValues << int16Le(offset);
        const quint8 bands = static_cast<quint8>(packet.at(23));
        currentValues << (bands & 0x0f) << ((bands >> 4) & 0x0f);
        currentValues << static_cast<qint8>(packet.at(24)) << static_cast<qint8>(packet.at(25));
        const quint8 switches = static_cast<quint8>(packet.at(26));
        const quint8 buttons = static_cast<quint8>(packet.at(27));
        currentValues << ((switches >> 6) & 1)
                      << ((buttons >> 7) & 1)
                      << ((buttons >> 6) & 1);
        for (int bit = 4; bit >= 0; --bit)
            currentValues << ((switches >> bit) & 1);
        for (int bit = 5; bit >= 0; --bit)
            currentValues << ((buttons >> bit) & 1);
    }
    if (currentValues.size() != decodedTable->rowCount()) return;
    for (int row = 0; row < currentValues.size(); ++row) {
        const bool digital = row >= 13;
        decodedTable->item(row, 1)->setText(digital
            ? (currentValues.at(row) ? tr("ON") : tr("OFF"))
            : QString::number(currentValues.at(row)));
        bool changed = false;
        if (baselineCaptured) {
            const qint64 delta = currentValues.at(row) - baselineValues.at(row);
            changed = row < 9 ? qAbs(delta) >= analogThreshold->value() : delta != 0;
            decodedTable->item(row, 3)->setText(digital
                ? (changed ? tr("YES") : tr("no"))
                : QStringLiteral("%1%2").arg(delta >= 0 ? "+" : "").arg(delta));
        } else {
            decodedTable->item(row, 3)->setText(QStringLiteral("—"));
        }
        const QBrush brush = changed ? QBrush(QColor(170, 255, 180)) : QBrush();
        for (int column = 0; column < decodedTable->columnCount(); ++column)
            decodedTable->item(row, column)->setBackground(brush);
    }
}

void PathfinderReceiver::captureBaseline()
{
    if (currentValues.size() != decodedTable->rowCount()) {
        status->setText(tr("No valid packet yet; wait for data before capturing a baseline."));
        return;
    }
    baselineValues = currentValues;
    baselineCaptured = true;
    for (int row = 0; row < baselineValues.size(); ++row) {
        const bool digital = row >= 13;
        decodedTable->item(row, 2)->setText(digital
            ? (baselineValues.at(row) ? tr("ON") : tr("OFF"))
            : QString::number(baselineValues.at(row)));
    }
    updateDecodedValues(QByteArray());
    status->setText(tr("Baseline captured. Move exactly one control; changed candidates will turn green."));
}

void PathfinderReceiver::toggleRecording()
{
    if (recording.isOpen()) { stopRecording(); return; }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save received raw bytes"),
                                                     "pathfinder.bin", tr("Binary data (*.bin)"));
    if (path.isEmpty()) return;
    // The dialog runs an event loop: the device may have disconnected meanwhile.
    if (!serial.isOpen()) return;
    recording.setFileName(path);
    if (!recording.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Cannot record"), recording.errorString());
        return;
    }
    recordButton->setText(tr("Stop recording"));
    status->setText(tr("Recording raw bytes to %1").arg(path));
}

void PathfinderReceiver::stopRecording()
{
    if (recording.isOpen()) {
        recording.close();
        status->setText(tr("Recording saved: %1").arg(recording.fileName()));
    }
    recordButton->setText(tr("Record raw data..."));
}
