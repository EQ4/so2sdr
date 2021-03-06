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
#include "contest_fd.h"
#include "log.h"

/*! ARRL Field Day */
FD::FD()
{
    setVExch(false);
    dupeCheckingEveryBand = true;
    nExch                 = 2;
    logFieldPrefill       = new bool[nExch];
    logFieldPrefill[0]    = true;
    logFieldPrefill[1]    = true;
    prefill               = false;
    finalExch             = new QByteArray[nExch];
    exchange_type         = new FieldTypes[nExch];
    exchange_type[0]      = General;     // class
    exchange_type[1]      = ARRLSection; // section
    multFieldHighlight[0] = -1;          // no mults in FD
    multFieldHighlight[1] = -1;
}

FD::~FD()
{
    delete[] logFieldPrefill;
    delete[] finalExch;
    delete[] exchange_type;
}

QVariant FD::columnName(int c) const
{
    switch (c) {
    case SQL_COL_RCV1:return(QVariant("Class"));break;
    }
    return Contest::columnName(c);
}

void FD::addQso(Qso *qso)

// determine qso point value, increase nqso, update score
// update mult count
{
    if (!qso->dupe && qso->valid && qso->band<N_BANDS_SCORED) {
        // SSB=1 CW/RTTY=2
        switch (qso->mode) {
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
            qso->pts = 2;
            break;
        case RIG_MODE_USB:
        case RIG_MODE_LSB:
        case RIG_MODE_AM:
        case RIG_MODE_FM:
            qso->pts = 1;
            break;
        default:
            qso->pts = 1;
            break;
        }
        qsoPts += qso->pts;
    } else {
        qso->pts = 0;
    }
    addQsoMult(qso);
}

int FD::fieldWidth(int col) const

// width in pixels of data fields shown
{
    switch (col) {
    case 0: return(49); break; // rcv class
    case 1: return(35); break; // rcv section
    default: return(35);
    }
    return(0);
}

/*!
   number shown in column 0 is sequential qso number
 */
int FD::numberField() const
{
    return(-1);
}

unsigned int FD::rcvFieldShown() const

// 0 1=Class --> show
// 1 2=Section --> show
{
    return(1 + 2);  // show 2
}

int FD::Score() const
{
    return(qsoPts); // no mults
}

void FD::setupContest(QByteArray MultFile[MMAX], const Cty *cty)
{
    // keeps track of arrl sections, but score is only based on number of qsos
    if (MultFile[0].isEmpty()) {
        MultFile[0] = "arrl.txt";
    }
    readMultFile(MultFile, cty);
    zeroScore();
}

unsigned int FD::sntFieldShown() const
{
    return(0); // nothing shown
}

bool FD::validateExchange(Qso *qso)
{
    if (!separateExchange(qso)) return(false);
    if (exchElement.size() < 2) return(false);
    bool ok = false;

    for (int ii = 0; ii < MMAX; ii++) qso->mult[ii] = -1;

    // get the exchange
    determineMultType(qso);

    if (qso->isamult[0]) {
        // Domestic call: CLASS SECTION
        ok = valExch_name_state(0, qso->mult[0]);
    } else {
        // DX: CLASS DX
        finalExch[0] = exchElement[0];
        finalExch[1] = "DX";
        ok           = true;
    }
    for (int i = 0; i < nExch; i++) {
        qso->rcv_exch[i] = finalExch[i];
    }
    return(ok);
}
