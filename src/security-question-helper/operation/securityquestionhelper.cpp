// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "securityquestionhelper.h"
#include "dbus_service.h"
#include "pwqualitymanager.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

SecurityQuestionHelper::SecurityQuestionHelper(const QJsonObject &payload, QObject *parent)
    : QObject(parent)
    , m_isLock(payload.value(QStringLiteral("isLock")).toBool())
    , m_uid(payload.value(QStringLiteral("uid")).toInt(-1))
    , m_userName(payload.value(QStringLiteral("userName")).toString())
    , m_fullName(payload.value(QStringLiteral("fullName")).toString())
{
    m_ctx.uid = QString::number(m_uid);
    m_ctx.userName = m_userName;
    m_ctx.fullName = m_fullName;
    m_ctx.dbusObjectPath = QStringLiteral("/org/deepin/dde/Accounts1/User%1").arg(m_uid);
    m_ctx.isLock = m_isLock;

    for (const QJsonValue &v : payload.value(QStringLiteral("questions")).toArray())
        m_questions.append(v.toInt());

    const QString unlockTimeStr = payload.value(QStringLiteral("unlockTime")).toString();
    const bool payloadLocked = payload.value(QStringLiteral("locked")).toBool(false);

    // 防御性处理：payload 未标记锁定但后端实际已锁定
    if (!payloadLocked) {
        SqLimitsInfo limitsInfo = querySqLimits(m_ctx.dbusObjectPath);
        if (limitsInfo.maxTries - limitsInfo.numFailures <= 0 && limitsInfo.locked) {
            m_locked = true;
            m_unlockTime = limitsInfo.unlockTime;
            m_remaining = 0;
        } else {
            m_remaining = limitsInfo.maxTries - limitsInfo.numFailures;
        }
    } else {
        m_locked = true;
        m_unlockTime = unlockTimeStr;
        m_remaining = 0;
    }

    m_imeBackend = detectInputMethodBackend();
}

void SecurityQuestionHelper::submitAnswers(const QVariantMap &answers)
{
    m_ctx.verifiedAnswers.clear();
    for (auto it = answers.constBegin(); it != answers.constEnd(); ++it)
        m_ctx.verifiedAnswers.insert(it.key().toInt(), it.value().toString());

    QDBusPendingCall pending = verifySecretQuestions(m_ctx);
    if (pending.isError()) {
        qWarning() << "[SQ-Helper] verifySecretQuestions async failed:" << pending.error().message();
        refreshLimitsAsync();
        return;
    }

    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusMessage reply = w->reply();

        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "[SQ-Helper] VerifySecretQuestionsForReset failed:" << reply.errorMessage();
            refreshLimitsAsync();
            return;
        }

        const QList<QVariant> args = reply.arguments();
        if (args.isEmpty()) {
            qWarning() << "[SQ-Helper] VerifySecretQuestionsForReset empty reply";
            refreshLimitsAsync();
            return;
        }

        const QList<int> failed = qdbus_cast<QList<int>>(args.at(0));
        if (failed.isEmpty()) {
            qInfo() << "[SQ-Helper] Verification passed";
            Q_EMIT verificationFinished(true, m_remaining, m_locked, m_unlockTime);
            return;
        }

        qInfo() << "[SQ-Helper] Verification failed, incorrect answers count:" << failed.size();
        // 每次失败后从后端查询最新限制状态
        refreshLimitsAsync();
    });
}

void SecurityQuestionHelper::refreshLimitsAsync()
{
    QDBusPendingCall pending = querySqLimitsAsync(m_ctx.dbusObjectPath);
    if (pending.isError()) {
        qWarning() << "[SQ-Helper] querySqLimitsAsync failed:" << pending.error().message();
        Q_EMIT verificationFinished(false, m_remaining, m_locked, m_unlockTime);
        return;
    }

    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusMessage reply = w->reply();

        SqLimitsInfo limitsInfo;
        const QList<QVariant> args = reply.arguments();
        if (reply.type() == QDBusMessage::ReplyMessage && args.size() >= 4) {
            limitsInfo.locked = args.at(0).toBool();
            limitsInfo.maxTries = args.at(1).toInt();
            limitsInfo.numFailures = args.at(2).toInt();
            limitsInfo.unlockTime = args.at(3).toString();
            qInfo() << "[SQ-Helper] querySqLimits: locked=" << limitsInfo.locked
                    << "maxTries=" << limitsInfo.maxTries << "numFailures=" << limitsInfo.numFailures
                    << "unlockTime=" << limitsInfo.unlockTime;
        } else {
            qWarning() << "[SQ-Helper] querySqLimits failed:" << reply.errorMessage();
        }

        const int remaining = limitsInfo.maxTries - limitsInfo.numFailures;
        if (limitsInfo.locked && !limitsInfo.unlockTime.isEmpty()) {
            m_locked = true;
            m_unlockTime = limitsInfo.unlockTime;
            m_remaining = 0;
        } else {
            m_locked = false;
            m_unlockTime.clear();
            m_remaining = remaining;
        }
        Q_EMIT lockedChanged();
        Q_EMIT remainingAttemptsChanged();
        Q_EMIT verificationFinished(false, m_remaining, m_locked, m_unlockTime);
    });
}

int SecurityQuestionHelper::passwordLevel(const QString &pwd)
{
    return static_cast<int>(dccV25::PwqualityManager::instance()->GetNewPassWdLevel(pwd));
}

QString SecurityQuestionHelper::checkPassword(const QString &fullName,
                                const QString &user,
                                const QString &pwd)
{
    // Keep the same validation order as the legacy password dialog. The
    // pw-check version used by this branch exposes PW_ERR_SAME_AS_USERNAME
    // instead of the newer PW_ERR_PART_OF_USERNAME.
    auto *pwManager = dccV25::PwqualityManager::instance();
    auto error = pwManager->verifyPassword(fullName, pwd);
    // Match the legacy password dialog: only preserve the username-related
    // error from the full-name check; otherwise validate against username.
    if (error != PW_ERR_SAME_AS_USERNAME)
        error = pwManager->verifyPassword(user, pwd);

    if (error != dccV25::PwqualityManager::ERROR_TYPE::PW_NO_ERR)
        return pwManager->getErrorTips(error);
    return QString();
}

void SecurityQuestionHelper::resetPassword(const QString &newPwd, const QString &hint)
{
    qInfo() << "[SQ-Helper] Calling resetPassword (async)";
    QDBusPendingCall pending = resetPasswordWithToken(m_ctx, newPwd);
    if (pending.isError()) {
        qWarning() << "[SQ-Helper] resetPasswordWithToken async failed";
        Q_EMIT resetFinished(false, tr("Failed to reset password, please try again."));
        return;
    }

    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, hint](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusMessage reply = w->reply();

        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "[SQ-Helper] ResetPassword failed:" << reply.errorMessage();
            Q_EMIT resetFinished(false, tr("Failed to reset password, please try again."));
            return;
        }

        qInfo() << "[SQ-Helper] Password changed successfully";
        setPasswordHint(m_ctx, hint);

        if (!m_isLock) {
            QDBusInterface login1User(QStringLiteral("org.freedesktop.login1"),
                                      QStringLiteral("/org/freedesktop/login1/user/%1").arg(m_ctx.uid),
                                      QStringLiteral("org.freedesktop.login1.User"),
                                      QDBusConnection::systemBus());
            if (login1User.isValid()) {
                QDBusReply<void> termReply = login1User.call(QStringLiteral("Terminate"));
                if (!termReply.isValid())
                    qWarning() << "[SQ-Helper] Terminate user session failed:" << termReply.error().message();
            }
        }

        Q_EMIT resetFinished(true, QString());
    });
}

void SecurityQuestionHelper::releaseInputGrabs()
{
    ++m_grabGeneration;
    if (m_grabWindow) {
        m_grabWindow->setKeyboardGrabEnabled(false);
        m_grabWindow->setMouseGrabEnabled(false);
    }
    if (m_pendingGrabWindow && m_pendingGrabWindow != m_grabWindow) {
        m_pendingGrabWindow->setKeyboardGrabEnabled(false);
        m_pendingGrabWindow->setMouseGrabEnabled(false);
    }
    m_grabWindow.clear();
    m_pendingGrabWindow.clear();
}

void SecurityQuestionHelper::releaseAllInputGrabs()
{
    releaseInputGrabs();
    if (QInputMethod *inputMethod = QGuiApplication::inputMethod()) {
        inputMethod->hide();
        inputMethod->reset();
    }
}

void SecurityQuestionHelper::cancelFlow()
{
    if (m_finished)
        return;
    invalidateSession(m_ctx);
    finish(false, false, false);
}

void SecurityQuestionHelper::finish(bool accepted, bool verified, bool passwordChanged)
{
    // 防止取消/关闭路径多次导致 stdout 写入重复 JSON
    // （取消按钮 onClicked 与窗口 onClosing 都可能触发 cancelled()）
    if (m_finished)
        return;
    m_finished = true;
    // 退出前先归还鼠标/键盘 grab，避免宿主（greeter/dde-lock）regrab 不上而卡死
    releaseAllInputGrabs();
    QJsonObject output;
    if (accepted) {
        output = {
            { QStringLiteral("accepted"), true },
            { QStringLiteral("verified"), verified },
            { QStringLiteral("passwordChanged"), passwordChanged }
        };
    } else {
        output = { { QStringLiteral("accepted"), false } };
    }
    QTextStream out(stdout);
    out << QString::fromUtf8(QJsonDocument(output).toJson(QJsonDocument::Compact))
        << Qt::endl;
    out.flush();
    Q_EMIT finished();
}

void SecurityQuestionHelper::setupIme(QQuickItem *item)
{
    setupImeForItem(m_imeBackend, item);
}

void SecurityQuestionHelper::refreshIme(QQuickItem *item)
{
    refreshInputMethod(item);
}

void SecurityQuestionHelper::activateAndGrabInput(QObject *winObj)
{
    auto *win = qobject_cast<QQuickWindow *>(winObj);
    if (!win) {
        qWarning() << "[SQ-Helper] activateAndGrabInput received a non-QQuickWindow object:"
                   << (winObj ? winObj->metaObject()->className() : "null");
        return;
    }

    if (isWayland())
        win->setProperty("_d_dwayland_window-type", "onScreenDisplay");

    releaseInputGrabs();
    m_pendingGrabWindow = win;
    const quint64 generation = m_grabGeneration;

    qInfo() << "[SQ-Helper] activateAndGrabInput: window=" << win
            << "visible=" << win->isVisible()
            << "isActive=" << win->isActive()
            << "platform=" << QGuiApplication::platformName();

    win->raise();
    win->requestActivate();

    qInfo() << "[SQ-Helper] after raise/requestActivate: isActive=" << win->isActive()
            << "isExposed=" << win->isExposed();

    if (isWayland())
        return;

    QTimer::singleShot(0, win, [this, win, generation] {
        scheduleGrab(win, generation, 0);
    });
}

void SecurityQuestionHelper::scheduleGrab(QQuickWindow *win, quint64 generation, int attempt)
{
    constexpr int maxAttempts = 3;
    constexpr int retryIntervalMs = 50;
    if (m_finished || generation != m_grabGeneration
        || win != m_pendingGrabWindow || !win)
        return;

    if (!win->isVisible() || !win->isExposed()) {
        if (attempt + 1 < maxAttempts) {
            QTimer::singleShot(retryIntervalMs, win, [this, win, generation, attempt] {
                scheduleGrab(win, generation, attempt + 1);
            });
        } else {
            qWarning() << "[SQ-Helper] X11 input grab failed: window not ready"
                       << "visible=" << win->isVisible()
                       << "exposed=" << win->isExposed();
            finish(false, false, false);
        }
        return;
    }

    win->raise();
    win->requestActivate();
    const bool mouseGrabbed = win->setMouseGrabEnabled(true);
    const bool keyboardGrabbed = win->setKeyboardGrabEnabled(true);
    if (mouseGrabbed && keyboardGrabbed) {
        m_grabWindow = win;
        m_pendingGrabWindow.clear();
        qInfo() << "[SQ-Helper] X11 input grab acquired successfully";
        Q_EMIT inputGrabAcquired();
        return;
    }

    if (mouseGrabbed)
        win->setMouseGrabEnabled(false);
    if (keyboardGrabbed)
        win->setKeyboardGrabEnabled(false);
    if (attempt + 1 >= maxAttempts) {
        qWarning() << "[SQ-Helper] X11 input grab failed after" << maxAttempts
                    << "attempts; closing helper to avoid an unprotected dialog."
                    << "mouse:" << mouseGrabbed << "keyboard:" << keyboardGrabbed;
        finish(false, false, false);
        return;
    }

    QTimer::singleShot(retryIntervalMs, win, [this, win, generation, attempt] {
        scheduleGrab(win, generation, attempt + 1);
    });
}
