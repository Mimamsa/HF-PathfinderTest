/*
 * HF-PathfinderTest
 * Derived from RF-TrulyMagical, Copyright (C) 2014 Zain Mir.
 * Licensed under the GNU General Public License version 3 or later.
 */
#include <QApplication>
#include <QCoreApplication>
#include "PathfinderReceiver.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HF-PathfinderTest"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    PathfinderReceiver receiver;
    receiver.setWindowTitle(QStringLiteral("HF-PathfinderTest"));
    receiver.resize(1120, 720);
    receiver.show();
    return app.exec();
}
