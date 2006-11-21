/**
 * Copyright (c) 2006 Benjamin C. Meyer (ben at meyerhome dot net)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Benjamin Meyer nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "probe.h"
#include <qvector.h>
#include <qdebug.h>

#ifdef Q_OS_WIN
#include <winmmap.h>
#else
#include <sys/mman.h>
#endif

#define PROBEFILENAME "probe"

bool Probe::readProbeData(const QString &probeFileName) {
    QVector<uint> probeData;
    QFile data(probeFileName);
    if (!data.open(QFile::ReadOnly)) {
        qWarning() << "Error: Unable to open probe file." << probeFileName;
        return false;
    }

    uint movie = 0;
    QTextStream in(&data);
    QString line;
    while (!in.atEnd()) {
        line = in.readLine();
        if (line.right(1) == ":") {
            movie = line.mid(0, line.length()-1).toInt();
            if (movie <= 0) {
                qWarning() << "Error: Found movie with id 0 in probe file.";
                return false;
            }
            probeData.append(0);
            probeData.append(movie);
        } else {
            int realValue = 0;
            int user = 0;
            if (line[1] == ',') {
                realValue = QString(line[0]).toInt();
                user = line.mid(2).toInt();
            } else {
                user = line.toInt();
                Movie m(db, movie);
                realValue = m.findScore(user);
            }
            if (user <= 0) {
                qWarning() << "Error: Found user with id 0 in probe file.";
                return false;
            }
            probeData.append(user);
            probeData.append(realValue);
        }
    }
    DataBase::saveDatabase(probeData, db->rootPath() + "/" + PROBEFILENAME + ".data");
    qDebug() << "probe data saved to a database";
    return true;
}

int Probe::runProbe(Algorithm *algorithm, const QString &probeFileName)
{
    QString fileName = probeFileName;
    if (fileName.isEmpty())
        fileName = db->rootPath() + "/" + PROBEFILENAME;
    if (!db->isLoaded())
        return -1;
    if (!QFile::exists(fileName + ".data"))
        if (!readProbeData(fileName + ".txt"))
            return -1;

    uint *probe;
    uint probeSize = 0;

    QFile file(fileName + ".data");
    if (file.size() != 0
        && file.exists()
        && file.open(QFile::ReadOnly | QFile::Unbuffered)) {
        probe = (uint*)
                mmap(0, file.size(), PROT_READ, MAP_SHARED,
                  file.handle(),
                   (off_t)0);
        if (probe == (uint*)-1) {
            qWarning() << "probe mmap failed";
            return -1;
        }
        probeSize = file.size() / sizeof(uint);
    } else {
        qWarning() << "unable to load probe database";
        return -1;
    }

    RMSE rmse;
    int total = 1408395;
    int percentDone = 0;
    int currentMovie = -1;
    for (uint i = 0; i < probeSize; ++i) {
        if (probe[i] == 0) {
            currentMovie = -1;
            continue;
        }
        if (currentMovie == -1) {
            currentMovie = probe[i];
            algorithm->setMovie(currentMovie);
            continue;
        }
        int user = probe[i++];
        int realValue = probe[i];
        double guess = algorithm->determine(user);
        //if (guess != realValue)
        //    qDebug() << "movie:" << currentMovie << "user:" << user << "guess:" << guess << "correct:" << realValue;
        rmse.addPoint(realValue, guess);
        int t = rmse.count() / (total/100);
        if (t != percentDone) {
            percentDone = t;
            qDebug() << rmse.count() << percentDone << "%" << rmse.result();
        }
    }

    qDebug() << rmse.result();
    return 0;
}

