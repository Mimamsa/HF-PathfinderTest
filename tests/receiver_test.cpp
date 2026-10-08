#include <QtTest>
#include <QComboBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QTableWidget>
#include "PathfinderReceiver.h"
#ifdef Q_OS_LINUX
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#endif

class ReceiverTest : public QObject
{
    Q_OBJECT
private slots:
    void defaultModeAndFailedOpen()
    {
        PathfinderReceiver receiver;
        auto *baud = receiver.findChild<QComboBox *>("pathfinderBaud");
        QCOMPARE(baud->currentData().toInt(), 115200);
        auto *port = receiver.findChild<QComboBox *>("pathfinderPort");
        port->setCurrentText("/dev/pathfinder-nonexistent-test");
        auto *button = receiver.findChild<QPushButton *>("pathfinderConnect");
        button->click();
        QCOMPARE(button->text(), QString("Connect"));
        QVERIFY(port->isEnabled());
        QVERIFY(receiver.findChild<QLabel *>("pathfinderStatus")->text().startsWith("Cannot open"));
    }

    void optionalHardwareReception()
    {
        const QString name = qEnvironmentVariable("PATHFINDER_TEST_PORT");
        if (name.isEmpty()) QSKIP("Set PATHFINDER_TEST_PORT for a live receive-only test");
        PathfinderReceiver receiver;
        receiver.resize(1050, 620);
        receiver.show();
        receiver.findChild<QComboBox *>("pathfinderPort")->setCurrentText(name);
        auto *button = receiver.findChild<QPushButton *>("pathfinderConnect");
        button->click();
        QCOMPARE(button->text(), QString("Disconnect"));
        auto *serial = receiver.findChild<QSerialPort *>();
        QVERIFY(serial);
        QVERIFY(!serial->isRequestToSend());
        auto *output = receiver.findChild<QPlainTextEdit *>("pathfinderOutput");
        QTRY_VERIFY_WITH_TIMEOUT(output->document()->blockCount() > 1, 5000);
        qInfo().noquote() << output->toPlainText().left(600);
        receiver.grab().save("/tmp/pathfinder-receiver-preview.png");
        button->click();
        QCOMPARE(button->text(), QString("Connect"));
    }

    void receiveWithoutTransmittingAndReconnect()
    {
#ifndef Q_OS_LINUX
        QSKIP("Pseudo-terminal integration test requires Linux");
#else
        int master = -1, slave = -1;
        char name[128] = {};
        QVERIFY(openpty(&master, &slave, name, nullptr, nullptr) == 0);
        struct Cleanup { int &master; int slave; ~Cleanup() { if (master >= 0) ::close(master); ::close(slave); } } cleanup{master, slave};
        QVERIFY(fcntl(master, F_SETFL, O_NONBLOCK) != -1);
        PathfinderReceiver receiver;
        auto *ports = receiver.findChild<QComboBox *>("pathfinderPort");
        auto *button = receiver.findChild<QPushButton *>("pathfinderConnect");
        auto *output = receiver.findChild<QPlainTextEdit *>("pathfinderOutput");
        ports->setCurrentText(QString::fromLocal8Bit(name));
        for (int attempt = 0; attempt < 2; ++attempt) {
            button->click();
            QCOMPARE(button->text(), QString("Disconnect"));
            QVERIFY(!ports->isEnabled());
            auto makePacket = [](bool pressed, quint8 switches = 0, quint8 upperButtons = 0) {
                QByteArray packet = QByteArray::fromHex("aa0f012f1703000400030002000100feff00000000000011000000");
                packet[26] = static_cast<char>(switches);
                packet.append(char(upperButtons | (pressed ? 0x20 : 0x00)));
                quint16 crc = 0;
                for (const char byte : packet) {
                    crc ^= quint16(quint8(byte)) << 8;
                    for (int bit = 0; bit < 8; ++bit)
                        crc = (crc & 0x8000) ? quint16((crc << 1) ^ 0x1021) : quint16(crc << 1);
                }
                packet.append(char(crc & 0xff)); packet.append(char(crc >> 8)); packet.append(char(0x0d));
                return packet;
            };
            const QByteArray released = makePacket(false);
            QCOMPARE(::write(master, released.constData(), released.size()), ssize_t(released.size()));
            QTRY_VERIFY(output->toPlainText().contains("BUT1=released"));
            auto *table = receiver.findChild<QTableWidget *>("pathfinderDecoded");
            QVERIFY(table);
            QCOMPARE(table->item(21, 0)->text(), QString("BUT1"));
            QCOMPARE(table->item(21, 1)->text(), QString("OFF"));
            receiver.findChild<QPushButton *>("pathfinderBaseline")->click();
            QCOMPARE(table->item(21, 2)->text(), QString("OFF"));
            const QByteArray pressed = makePacket(true);
            QCOMPARE(::write(master, pressed.constData(), pressed.size()), ssize_t(pressed.size()));
            QTRY_VERIFY(output->toPlainText().contains("BUT1=PRESSED"));
            QCOMPARE(receiver.findChild<QLabel *>("pathfinderBut1")->text(), QString("BUT1: PRESSED"));
            QCOMPARE(table->item(21, 1)->text(), QString("ON"));
            QCOMPARE(table->item(21, 3)->text(), QString("YES"));
            const QByteArray joy1d = makePacket(false, 0x40);
            QCOMPARE(::write(master, joy1d.constData(), joy1d.size()), ssize_t(joy1d.size()));
            QTRY_COMPARE(table->item(13, 1)->text(), QString("ON"));
            QCOMPARE(table->item(14, 1)->text(), QString("OFF"));
            const QByteArray joy2d = makePacket(false, 0x00, 0x80);
            QCOMPARE(::write(master, joy2d.constData(), joy2d.size()), ssize_t(joy2d.size()));
            QTRY_COMPARE(table->item(14, 1)->text(), QString("ON"));
            QCOMPARE(table->item(15, 1)->text(), QString("OFF"));
            const QByteArray joy3d = makePacket(false, 0x00, 0x40);
            QCOMPARE(::write(master, joy3d.constData(), joy3d.size()), ssize_t(joy3d.size()));
            QTRY_COMPARE(table->item(15, 1)->text(), QString("ON"));
            QCOMPARE(table->item(14, 1)->text(), QString("OFF"));
            char data[16];
            QCOMPARE(::read(master, data, sizeof(data)), ssize_t(-1));
            QVERIFY(errno == EAGAIN || errno == EWOULDBLOCK);
            button->click();
            QCOMPARE(button->text(), QString("Connect"));
            QVERIFY(ports->isEnabled());
            output->clear();
        }
        button->click();
        // PTY hangup notification differs across Qt versions; inject the same
        // ResourceError signal emitted by QSerialPort for a removed device.
        auto *serial = receiver.findChild<QSerialPort *>();
        QVERIFY(serial);
        serial->errorOccurred(QSerialPort::ResourceError);
        QTRY_COMPARE(button->text(), QString("Connect"));
        QVERIFY(receiver.findChild<QLabel *>("pathfinderStatus")->text().startsWith("Serial error"));
#endif
    }
};
QTEST_MAIN(ReceiverTest)
#include "receiver_test.moc"
