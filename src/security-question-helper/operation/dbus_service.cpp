// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dbus_service.h"

#include <QDateTime>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include <DConfig>

#include <crypt.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sys/mman.h>

DCORE_USE_NAMESPACE

namespace {
const QString AccountsService = QStringLiteral("org.deepin.dde.Accounts1");
const QString AccountsUserInterface = QStringLiteral("org.deepin.dde.Accounts1.User");

// 密码加密算法前缀映射，按优先级排序（从高到低）
// 注意：顺序除非产品要求不要变更（与 plugin-accounts 保持一致）
const QList<QPair<QString, QString>> kPasswordAlgorithmPrefixes = {
    { QStringLiteral("sm3"), QStringLiteral("$sm3$") },
    { QStringLiteral("yescrypt"), QStringLiteral("$y$") },
    { QStringLiteral("sha512"), QStringLiteral("$6$") },
    { QStringLiteral("sha256"), QStringLiteral("$5$") }
};

QString getAlgorithmPrefix(const QString &algorithm)
{
    for (const auto &pair : kPasswordAlgorithmPrefixes) {
        if (pair.first.toLower() == algorithm.toLower())
            return pair.second;
    }
    return QString();
}

QString tryEncryptPassword(const QString &password, const QString &algorithm)
{
    const QString saltPrefix = getAlgorithmPrefix(algorithm);
    if (saltPrefix.isEmpty())
        return QString();

    char output[CRYPT_GENSALT_OUTPUT_SIZE];
    char *setting = crypt_gensalt_rn(saltPrefix.toLatin1().data(), 0, nullptr, 0, output, sizeof(output));
    if (setting == nullptr || setting[0] == '*')
        return QString();

    char *result = crypt(password.toUtf8().data(), setting);
    if (result == nullptr || result[0] == '*')
        return QString();

    return QString(result);
}

int writePayloadToFd(const QByteArray &data)
{
    int fd = memfd_create("sq-payload", MFD_CLOEXEC);
    if (fd == -1)
        return -1;
    if (write(fd, data.constData(), static_cast<size_t>(data.size())) == -1
        || lseek(fd, 0, SEEK_SET) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}
} // namespace

QDBusPendingCall verifySecretQuestions(const ResetContext &ctx)
{
    QJsonObject answersObj;
    for (auto it = ctx.verifiedAnswers.constBegin(); it != ctx.verifiedAnswers.constEnd(); ++it)
        answersObj.insert(QString::number(it.key()), it.value());

    QJsonObject payload;
    payload[QStringLiteral("answers")] = answersObj;

    const QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    qWarning() << "[SQ-Helper] verifySecretQuestions: building memfd payload, dbus path:" << ctx.dbusObjectPath
               << "payload:" << QString::fromUtf8(data);
    int rawFd = writePayloadToFd(data);
    if (rawFd == -1) {
        qWarning() << "[SQ-Helper] memfd_create failed";
        return QDBusPendingCall::fromError(QDBusError(QDBusError::Failed, QStringLiteral("memfd create failed")));
    }

    QDBusInterface userInter(AccountsService, ctx.dbusObjectPath, AccountsUserInterface, QDBusConnection::systemBus());
    if (!userInter.isValid()) {
        qWarning() << "[SQ-Helper] DBus interface invalid for" << ctx.dbusObjectPath;
        close(rawFd);
        return QDBusPendingCall::fromError(QDBusError(QDBusError::Failed, QStringLiteral("invalid dbus interface")));
    }

    QDBusPendingCall pendingCall = userInter.asyncCall(
        "VerifySecretQuestionsForReset", QVariant::fromValue(QDBusUnixFileDescriptor(rawFd)));
    close(rawFd);

    if (pendingCall.isError()) {
        qWarning() << "[SQ-Helper] VerifySecretQuestionsForReset async error:" << pendingCall.error().message();
    } else {
        qWarning() << "[SQ-Helper] VerifySecretQuestionsForReset asyncCall issued";
    }

    return pendingCall;
}

QDBusPendingCall resetPasswordWithToken(const ResetContext &ctx, const QString &newPassword)
{
    QJsonObject payload;
    payload[QStringLiteral("newPassword")] = cryptUserPassword(newPassword);

    const QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    int rawFd = writePayloadToFd(data);
    if (rawFd == -1) {
        qWarning() << "[SQ-Helper] memfd_create failed";
        return QDBusPendingCall::fromError(QDBusError(QDBusError::Failed, QStringLiteral("memfd create failed")));
    }

    QDBusInterface userInter(AccountsService, ctx.dbusObjectPath, AccountsUserInterface, QDBusConnection::systemBus());
    if (!userInter.isValid()) {
        qWarning() << "[SQ-Helper] DBus interface invalid for" << ctx.dbusObjectPath;
        close(rawFd);
        return QDBusPendingCall::fromError(QDBusError(QDBusError::Failed, QStringLiteral("invalid dbus interface")));
    }

    QDBusPendingCall pendingCall = userInter.asyncCall(
        "ResetPassword", QVariant::fromValue(QDBusUnixFileDescriptor(rawFd)));
    close(rawFd);

    if (pendingCall.isError()) {
        qWarning() << "[SQ-Helper] ResetPassword async error:" << pendingCall.error().message();
    }

    return pendingCall;
}

void setPasswordHint(const ResetContext &ctx, const QString &hint)
{
    if (hint.isEmpty())
        return;

    QDBusInterface userInter(AccountsService, ctx.dbusObjectPath, AccountsUserInterface, QDBusConnection::systemBus());
    if (!userInter.isValid()) {
        qWarning() << "[SQ-Helper] setPasswordHint: unable to connect to account service";
        return;
    }

    QDBusReply<void> reply = userInter.call("SetPasswordHint", hint);
    if (!reply.isValid())
        qWarning() << "[SQ-Helper] SetPasswordHint failed:" << reply.error().message();
    else
        qInfo() << "[SQ-Helper] Password hint set successfully";
}

void invalidateSession(const ResetContext &ctx)
{
    QDBusInterface userInter(AccountsService, ctx.dbusObjectPath, AccountsUserInterface, QDBusConnection::systemBus());
    if (!userInter.isValid())
        return;

    // Session invalidation is best-effort during shutdown. Do not block the
    // greeter/lock screen while waiting for AccountsService to reply.
    userInter.asyncCall(QStringLiteral("InvalidateVerificationSession"));
}

SqLimitsInfo querySqLimits(const QString &dbusObjectPath)
{
    SqLimitsInfo info;
    QDBusMessage msg = QDBusMessage::createMethodCall(AccountsService, dbusObjectPath, AccountsUserInterface, "GetSqLimits");
    QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() >= 4) {
        info.locked = reply.arguments().at(0).toBool();
        info.maxTries = reply.arguments().at(1).toInt();
        info.numFailures = reply.arguments().at(2).toInt();
        info.unlockTime = reply.arguments().at(3).toString();
        qInfo() << "[SQ-Helper] querySqLimits: locked=" << info.locked
                << "maxTries=" << info.maxTries << "numFailures=" << info.numFailures
                << "unlockTime=" << info.unlockTime;
    } else {
        qWarning() << "[SQ-Helper] querySqLimits failed:" << reply.errorMessage();
    }
    return info;
}

QDBusPendingCall querySqLimitsAsync(const QString &dbusObjectPath)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(AccountsService, dbusObjectPath, AccountsUserInterface, "GetSqLimits");
    return QDBusConnection::systemBus().asyncCall(msg);
}

QString cryptUserPassword(const QString &password)
{
    // 从 dconfig 获取加密算法，如果获取失败或不存在则默认为 sm3
    QString algorithm = QStringLiteral("sm3");
    DConfig *accountCfg = DConfig::create(QStringLiteral("org.deepin.dde.daemon"),
                                          QStringLiteral("org.deepin.dde.daemon.account"),
                                          QString(), nullptr);
    if (accountCfg && accountCfg->isValid()) {
        algorithm = accountCfg->value(QStringLiteral("passwordEncryptionAlgorithm"), QStringLiteral("sm3")).toString();
        if (algorithm.isEmpty())
            algorithm = QStringLiteral("sm3");
    }

    QString encrypted = tryEncryptPassword(password, algorithm);
    if (accountCfg) {
        accountCfg->deleteLater();
    }
    if (!encrypted.isEmpty())
        return encrypted;

    qWarning() << "[SQ-Helper] Password encryption failed with configured algorithm:" << algorithm
               << ", trying fallback algorithms...";

    for (const auto &pair : kPasswordAlgorithmPrefixes) {
        const QString &fallbackAlg = pair.first;
        if (fallbackAlg.toLower() == algorithm.toLower())
            continue;

        encrypted = tryEncryptPassword(password, fallbackAlg);
        if (!encrypted.isEmpty()) {
            qWarning() << "[SQ-Helper] Password encryption succeeded with fallback algorithm:" << fallbackAlg;
            return encrypted;
        }
    }

    qCritical() << "[SQ-Helper] Password encryption failed with all supported algorithms!";
    return QString();
}
