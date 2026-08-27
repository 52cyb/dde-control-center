// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QObject>
#include <QString>

struct InputMethodBackend
{
    QString envName;
    QString executable;
    QString remoteCommand;
};

struct ResetContext
{
    QString uid;
    QString userName;
    QString fullName;
    QString dbusObjectPath;
    QMap<int, QString> verifiedAnswers;
    bool isLock = false;
};

struct SqLimitsInfo
{
    bool locked = false;
    int maxTries = 0;
    int numFailures = 0;
    QString unlockTime;
};

inline QString questionText(int index)
{
    switch (index) {
    case 1:
        return QObject::tr("What is the name of the city where you were born?");
    case 2:
        return QObject::tr("What is the name of your alma mater?");
    case 3:
        return QObject::tr("Who is the person you love the most?");
    case 4:
        return QObject::tr("What is your favorite animal?");
    case 5:
        return QObject::tr("What is your favorite music?");
    case 6:
        return QObject::tr("What is your nickname?");
    default:
        return QString();
    }
}
