/*! Copyright 2010-2015 R. Torsten Clay N4OGW

   This file is part of so2sdr.

    so2sdr is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    so2sdr is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with so2sdr.  If not, see <http://www.gnu.org/licenses/>.

 */
#ifndef TELNET_H
#define TELNET_H

#include <QSettings>
#include "ui_telnet.h"

class QtTelnet;

/*!
   DX Cluster Telnet window
 */
class Telnet : public QWidget, public Ui::TelnetDialog
{
Q_OBJECT

public:
    Telnet(QSettings& s,QWidget *parent = 0);
    ~Telnet();

signals:
    void done();
    void dxSpot(QByteArray, int);

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void connectTelnet();
    void disconnectTelnet();
    void sendText();
    void showText(QString txt);

private:
    QList<QString> hosts;
    QString        buffer;
    QSettings&     settings;
    QtTelnet       *telnet;
};
#endif // TELNET_H
