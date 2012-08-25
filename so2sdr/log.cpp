/*! Copyright 2010-2012 R. Torsten Clay N4OGW

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
#include <QFile>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlRecord>
#include "log.h"

/*!
   n_exch is number of exchange fields; this can't change during the contest
 */
log:: log(int n_exch, QObject *parent = 0) : QObject(parent)
{
    nExchange = n_exch;
    if (nExchange > MAX_EXCH_FIELDS) {
        nExchange = MAX_EXCH_FIELDS;
        qDebug("Warning: too many exchange fields in log()");
    }
    nrField        = -1;
    sntFieldsShown = 0;
    rcvFieldsShown = 0;
    prefill        = new bool[n_exch];
    for (int i = 0; i < n_exch; i++) prefill[i] = false;
    _qsoPtsField = false;
    rstField     = -1;
    logFileName.clear();
    db=new QSqlDatabase();
    *db = QSqlDatabase::addDatabase("QSQLITE");
}

log :: ~log()
{
    closeLogFile();
    delete db;
    delete [] prefill;
}

/*!
  close sql database
  */
void log::closeLogFile()
{
    db->close();
}

/*!
   ADIF file export

   @todo Handle non-standard bands
 */
bool log::exportADIF(QFile *adifFile) const
{
    QSqlQueryModel m;
    m.setQuery("SELECT * FROM log", *db);
    while (m.canFetchMore()) {
        m.fetchMore();
    }
    if (m.rowCount() == 0) {
        adifFile->close();
        return(false);  // nothing to do
    }

    bool ok = true;
    for (int i = 0; i < m.rowCount(); i++) {
        // do not include invalid qsos
        if (!m.record(i).value(SQL_COL_VALID).toBool()) continue;

        QByteArray tmp;
        QByteArray tmp2 = m.record(i).value(SQL_COL_CALL).toByteArray();

        // call
        tmp = "<CALL:" + QByteArray::number(tmp2.size()) + ">" + tmp2;

        // band
        switch (m.record(i).value(SQL_COL_BAND).toInt()) {
        case 0: tmp = tmp + "<BAND:4>160M"; break;
        case 1: tmp = tmp + "<BAND:3>80M"; break;
        case 2: tmp = tmp + "<BAND:3>40M"; break;
        case 3: tmp = tmp + "<BAND:3>20M"; break;
        case 4: tmp = tmp + "<BAND:3>15M"; break;
        case 5: tmp = tmp + "<BAND:3>10M"; break;
        }

        // frequency
        double f = m.record(i).value(SQL_COL_FREQ).toDouble() / 1000000.0;
        tmp2 = QString::number(f, 'f', 4).toAscii();
        tmp  = tmp + "<FREQ:" + QString::number(tmp2.size()).toAscii() + ">" + tmp2;

        // date
        // in SQL log, date is of format MMddyyyy; need yyyyMMdd for adif
        tmp2 = m.record(i).value(SQL_COL_DATE).toByteArray().right(4) +
               m.record(i).value(SQL_COL_DATE).toByteArray().left(2)
               + m.record(i).value(SQL_COL_DATE).toByteArray().mid(2, 2);
        tmp = tmp + "<QSO_DATE:8>" + tmp2;

        // time
        tmp = tmp + "<TIME_ON:4>" + m.record(i).value(SQL_COL_TIME).toByteArray();

        int rsti = 0;
        switch (m.record(i).value(SQL_COL_MODE).toInt()) {
        case RIG_MODE_LSB: case RIG_MODE_USB:
            tmp  = tmp + "<MODE:3>SSB";
            rsti = 1;
            break;
        case RIG_MODE_CW: case RIG_MODE_CWR:
            tmp = tmp + "<MODE:2>CW";
            break;
        case RIG_MODE_FM:
            tmp  = tmp + "<MODE:2>FM";
            rsti = 1;
            break;
        case RIG_MODE_AM:
            tmp  = tmp + "<MODE:2>AM";
            rsti = 1;
            break;
        case RIG_MODE_RTTY: case RIG_MODE_RTTYR:
            tmp = tmp + "<MODE:4>RTTY";
            break;
        default:
            tmp = tmp + "<MODE:2>CW";
            break;
        }

        // RS(T). If not in log, use 59/599
        if (rsti == 0) {
            // RST for CW, RTTY
            if (rstField == -1) {
                tmp = tmp + "<RST_SENT:3>599<RST_RCVD:3>599";
            } else {
                QByteArray tmp3;
                switch (rstField) {
                case 0:
                    tmp2 = m.record(i).value(SQL_COL_SNT1).toByteArray();
                    tmp3 = m.record(i).value(SQL_COL_RCV1).toByteArray(); break;
                case 1:
                    tmp2 = m.record(i).value(SQL_COL_SNT2).toByteArray();
                    tmp3 = m.record(i).value(SQL_COL_RCV2).toByteArray(); break;
                case 2:
                    tmp2 = m.record(i).value(SQL_COL_SNT3).toByteArray();
                    tmp3 = m.record(i).value(SQL_COL_RCV3).toByteArray(); break;
                case 3:
                    tmp2 = m.record(i).value(SQL_COL_SNT4).toByteArray();
                    tmp3 = m.record(i).value(SQL_COL_RCV4).toByteArray(); break;
                }
                tmp = tmp + "<RST_SENT:3>" + tmp2.left(3) + "<RST_RCVD:3>" + tmp3.left(3);
            }
        } else {
            // RS for voice modes
            if (rstField == -1) {
                tmp = tmp + "<RST_SENT:2>59<RST_RCVD:2>59";
            } else {
                QByteArray tmp3;
                switch (rstField) {
                case 0:
                    tmp2 = m.record(i).value(SQL_COL_SNT1).toByteArray();
                    tmp3 = m.record(i).value(SQL_COL_RCV1).toByteArray(); break;
                case 1:
                    tmp2 = m.record(i).value(SQL_COL_SNT2).toByteArray();
                    tmp3 = m.record(i).value(SQL_COL_RCV2).toByteArray(); break;
                case 2:
                    tmp2 = m.record(i).value(SQL_COL_SNT3).toByteArray();
                    tmp3 = m.record(i).value(SQL_COL_RCV3).toByteArray(); break;
                case 3:
                    tmp2 = m.record(i).value(SQL_COL_SNT4).toByteArray();
                    tmp3 = m.record(i).value(SQL_COL_RCV4).toByteArray(); break;
                }
                tmp = tmp + "<RST_SENT:2>" + tmp2.left(2) + "<RST_RCVD:2>" + tmp3.left(2);
            }
        }

        tmp = tmp + "<eor>\n";
        if (adifFile->write(tmp) == -1) {
            ok = false;
        }
    }
    adifFile->close();
    return(ok);
}

/*!
   Cabrillo export

 */
void log::exportCabrillo(QFile *cbrFile,QString call,QString snt_exch1,QString snt_exch2,QString snt_exch3,QString snt_exch4) const
{
    QSqlQueryModel m;
    m.setQuery("SELECT * FROM log", *db);

    while (m.canFetchMore()) {
        m.fetchMore();
    }
    if (m.rowCount() == 0) return;  // nothing to do

    // determine max field widths for sent/received data
    int sfw[MAX_EXCH_FIELDS], rfw[MAX_EXCH_FIELDS];
    for (int i = 0; i < MAX_EXCH_FIELDS; i++) {
        sfw[i] = 0.;
        rfw[i] = 0;
    }
    QByteArray snt[MAX_EXCH_FIELDS];
    for (int i = 0; i < m.rowCount(); i++) {
        // skip qsos marked as invalid
        if (!m.record(i).value(SQL_COL_VALID).toBool()) continue;

        // for sent exchange, if log field is empty use config file value
        snt[0]=m.record(i).value(SQL_COL_SNT1).toByteArray();
        if (snt[0].isEmpty()) snt[0]=snt_exch1.toAscii();
        int j = snt[0].size();
        if (j > sfw[0]) sfw[0] = j;

        snt[1]=m.record(i).value(SQL_COL_SNT2).toByteArray();
        if (snt[1].isEmpty()) snt[1]=snt_exch2.toAscii();
        j = snt[1].size();
        if (j > sfw[1]) sfw[1] = j;

        snt[2]=m.record(i).value(SQL_COL_SNT3).toByteArray();
        if (snt[2].isEmpty()) snt[2]=snt_exch3.toAscii();
        j = snt[2].size();
        if (j > sfw[2]) sfw[2] = j;

        snt[3]=m.record(i).value(SQL_COL_SNT4).toByteArray();
        if (snt[3].isEmpty()) snt[3]=snt_exch4.toAscii();
        j = snt[3].size();
        if (j > sfw[3]) sfw[3] = j;

        j = m.record(i).value(SQL_COL_RCV1).toByteArray().size();
        if (j > rfw[0]) rfw[0] = j;
        j = m.record(i).value(SQL_COL_RCV2).toByteArray().size();
        if (j > rfw[1]) rfw[1] = j;
        j = m.record(i).value(SQL_COL_RCV3).toByteArray().size();
        if (j > rfw[2]) rfw[2] = j;
        j = m.record(i).value(SQL_COL_RCV4).toByteArray().size();
    }

    for (int i = 0; i < m.rowCount(); i++) {
        // skip qsos marked as invalid
        if (!m.record(i).value(SQL_COL_VALID).toBool()) continue;

        // for sent exchange, if log field is empty use config file value
        snt[0]=m.record(i).value(SQL_COL_SNT1).toByteArray();
        if (snt[0].isEmpty()) snt[0]=snt_exch1.toAscii();

        snt[1]=m.record(i).value(SQL_COL_SNT2).toByteArray();
        if (snt[1].isEmpty()) snt[1]=snt_exch2.toAscii();

        snt[2]=m.record(i).value(SQL_COL_SNT3).toByteArray();
        if (snt[2].isEmpty()) snt[2]=snt_exch3.toAscii();

        snt[3]=m.record(i).value(SQL_COL_SNT4).toByteArray();
        if (snt[3].isEmpty()) snt[3]=snt_exch4.toAscii();

        QString tmp;
        tmp = "QSO: ";
        QString tmp2 = QString::number(qRound(m.record(i).value(SQL_COL_FREQ).toDouble() / 1000.0));
        for (int j = 0; j < (5 - tmp2.size()); j++) {
            tmp = tmp + " ";
        }
        tmp = tmp + tmp2;
        switch (m.record(i).value(SQL_COL_MODE).toInt()) {
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            tmp = tmp + " CW ";
            break;
        case RIG_MODE_USB:
        case RIG_MODE_LSB:
        case RIG_MODE_AM:
        case RIG_MODE_FM:
            tmp = tmp + " PH ";
            break;
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
            tmp = tmp + " RY ";
            break;
        default:
            tmp = tmp + " CW ";
            break;
        }

        // in SQL log, date is of format MMddyyyy
        tmp2 = m.record(i).value(SQL_COL_DATE).toByteArray();
        tmp  = tmp + tmp2.right(4) + "-" + tmp2.left(2) + "-" + tmp2.mid(2, 2);
        tmp  = tmp + " " + m.record(i).value(SQL_COL_TIME).toByteArray();
        tmp  = tmp + " " + call;
        int n = 11 - call.size();
        for (int j = 0; j < n; j++) tmp.append(" ");
        for (int j = 0; j < nExchange; j++) {
            QByteArray s;
            s.clear();
            switch (j) {
            case 0:
                s=snt[0];
                s = s.leftJustified(sfw[0], ' ');
                break;
            case 1:
                s=snt[1];
                s = s.leftJustified(sfw[1], ' ');
                break;
            case 2:
                s=snt[2];
                s = s.leftJustified(sfw[2], ' ');
                break;
            case 3:
                s=snt[3];
                s = s.leftJustified(sfw[3], ' ');
                break;
            }
            tmp = tmp + s + " ";
        }
        tmp = tmp + m.record(i).value(SQL_COL_CALL).toByteArray();
        n   = 11 - m.record(i).value(SQL_COL_CALL).toByteArray().size();
        for (int j = 0; j < n; j++) tmp.append(" ");
        for (int j = 0; j < nExchange; j++) {
            switch (j) {
            case 0:
                tmp = tmp + m.record(i).value(SQL_COL_RCV1).toByteArray().leftJustified(rfw[0], ' '); break;
            case 1:
                tmp = tmp + m.record(i).value(SQL_COL_RCV2).toByteArray().leftJustified(rfw[1], ' '); break;
            case 2:
                tmp = tmp + m.record(i).value(SQL_COL_RCV3).toByteArray().leftJustified(rfw[2], ' '); break;
            case 3:
                tmp = tmp + m.record(i).value(SQL_COL_RCV4).toByteArray().leftJustified(rfw[3], ' '); break;
            }
            tmp = tmp + " ";
        }
        tmp = tmp + "\n";
        cbrFile->write(tmp.toAscii());
    }
    cbrFile->write("END-OF-LOG:\n");
    cbrFile->close();
}

/*!
   Dupe checking subroutine.

   - qso->worked returns with bits set according to bands worked
   - DupeCheckingEveryBand controls whether calls can be worked on every band or only once.

   @todo Multiple-mode contests not implemented yet
 */
bool log::isDupe(Qso *qso, bool DupeCheckingEveryBand, bool FillWorked) const
{
    bool dupe = false;
    qso->worked = 0;
    qso->prefill.clear();
    QSqlQueryModel m;

    // call can only be worked once on any band
    if (!DupeCheckingEveryBand) {
        m.setQuery("SELECT * FROM log WHERE CALL='" + qso->call + "'", *db);
        while (m.canFetchMore()) {
            m.fetchMore();
        }
        if (m.rowCount()) {
            dupe = true;
            if (FillWorked) {
                // mult not needed on any band
                qso->worked = 63;
            }
        }
    } else {
        // if mobile station, check for mobile dupe option. In this
        // case, count dupe only if exchange is identical
        QString query="SELECT * FROM log WHERE call='" + qso->call + "' AND band=" + QString::number(qso->band);

        if (qso->isMobile && csettings->value(c_mobile_dupes,c_mobile_dupes_def).toBool()) {
            QString exch=qso->rcv_exch[csettings->value(c_mobile_dupes_col,c_mobile_dupes_col_def).toInt()-1];
            // if exchange not entered, can't determine dupe status yet
            if (exch.isEmpty()) {
                return(false);
            }
            // if qso already has an assigned number in log (which is SQL primary key), only check
            // qso's BEFORE this one
            if (qso->nr) {
                query=query+" AND (nr < "+QString::number(qso->nr)+") ";
            }

            query=query+ " AND ";
            switch (csettings->value(c_mobile_dupes_col,c_mobile_dupes_col_def).toInt()) {
            case 1:
                query=query+"rcv1='"+exch+"'";
                break;
            case 2:
                query=query+"rcv2='"+exch+"'";
                break;
            case 3:
                query=query+"rcv3='"+exch+"'";
                break;
            case 4:
                query=query+"rcv4='"+exch+"'";
                break;
            default:
                return(false);
            }
        }
        m.setQuery(query, *db);
        m.query().exec();
        while (m.canFetchMore()) {
            m.fetchMore();
        }
        if (m.rowCount()) {
            dupe=true;
        }
        if (FillWorked) {
            m.setQuery("SELECT * FROM log WHERE CALL='" + qso->call + "'", *db);
            for (int i = 0; i < m.rowCount(); i++) {
                qso->worked += bits[m.record(i).value(SQL_COL_BAND).toInt()];
            }
        }
    }
    // if a dupe, set zero pts
    if (dupe) qso->pts = 0;
    return(dupe);
}


/*!
   qso number sent for last qso in log
 */
int log::lastNr() const
{
    QSqlQueryModel m;
    m.setQuery("SELECT * FROM log", *db);
    while (m.canFetchMore()) {
        m.fetchMore();
    }
    if (m.rowCount()) {
        QByteArray snt[MAX_EXCH_FIELDS];
        snt[0] = m.record(m.rowCount() - 1).value(SQL_COL_SNT1).toByteArray();
        snt[1] = m.record(m.rowCount() - 1).value(SQL_COL_SNT2).toByteArray();
        snt[2] = m.record(m.rowCount() - 1).value(SQL_COL_SNT3).toByteArray();
        snt[3] = m.record(m.rowCount() - 1).value(SQL_COL_SNT4).toByteArray();
        bool ok = false;
        int  nr = snt[nrField].toInt(&ok, 10);
        if (!ok) nr = 0;
        return(nr);
    } else {
        return(0);
    }
}

/*!
slot for mobile dupe checking. Gets called by exchange validator if
exchange if ok. In this case, modify qso dupes status based on whether this
is a new mult or not
*/
void log::mobileDupeCheck(Qso *qso)
{
    qso->dupe=isDupe(qso,true,false);
}

/*!
   open log file on disk. If already exists, open as append.

   s is pointer to contest settings file
 */
bool log::openLogFile(QString fname,bool clear,QSettings *s)
{
    Q_UNUSED(clear);
    if (fname.isEmpty())
        return(false);

    csettings=s;
    logFileName = fname.remove(".cfg") + ".log";
    db->setDatabaseName(logFileName);
    if (!db->open()) {
        return(false);
    }
    QSqlQuery query(*db);

    if (!query.exec("create table if not exists log (nr int primary key,time text,freq int,call text,band int,date text,mode int,snt1 text,snt2 text,snt3 text,snt4 text,rcv1 text,rcv2 text,rcv3 text,rcv4 text,pts int,valid int)")) {
        return(false);
    }
    return(db->commit());
}

/*! show the qso points column on screen

   in contests where every qso is worth the same number of points, don't show it
 */
bool log::qsoPtsField() const
{
    return(_qsoPtsField);
}

/*!
   mark which fields are shown in on-screen log

   bit position=1 --> field visible
 */
void log::setFieldsShown(const unsigned int snt, const unsigned int rcv)
{
    sntFieldsShown = snt;  // for sent exchange
    rcvFieldsShown = rcv;  // for rcv exchange

    // count number of fields
    int cnt = 0;
    for (int i = 0; i < nExchange; i++) {
        if (sntFieldsShown & bits[i]) cnt++;
    }
    for (int i = 0; i < nExchange; i++) {
        if (rcvFieldsShown & bits[i]) cnt++;
    }
}

/*!
   Mark exchange field to be filled from previous log data
 */
void log::setPrefill(const int indx)
{
    prefill[indx] = true;
}

/*!
   ShowQsoPts indicates whether additional column shows the point value for each qso

 */
void log::setQsoPtsField(bool b)
{
    _qsoPtsField = b;
}

/*!
   Received exchange field which has RS(T). If == -1, no RST given, will assume 599
 */
void log::setRstField(int i)
{
    if (i < nExchange && i >= -1) {
        rstField = i;
    } else {
        rstField = -1;
    }
}


/*!
   Called to set which field contains the sent qso number
   this is needed so the sent # rather than sequential number can be shown in column 1
 */
void log::setupQsoNumbers(const int n)
{
    nrField = n;
}
