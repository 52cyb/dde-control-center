// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SECURITYQUESTIONHELPER_H
#define SECURITYQUESTIONHELPER_H

#include "sq_helper_types.h"
#include "ime_utils.h"

#include <QGuiApplication>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QVariantList>
#include <QVariantMap>

// 暴露给 QML 的 C++ 数据对象，提供 DBus 调用与流程状态（等价于插件中的 dccData）
class SecurityQuestionHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList questions READ questions CONSTANT)
    Q_PROPERTY(bool isLock READ isLock CONSTANT)
    Q_PROPERTY(QString userName READ userName CONSTANT)
    Q_PROPERTY(QString fullName READ fullName CONSTANT)
    Q_PROPERTY(bool locked READ locked NOTIFY lockedChanged)
    Q_PROPERTY(QString unlockTime READ unlockTime NOTIFY lockedChanged)
    Q_PROPERTY(int remainingAttempts READ remainingAttempts NOTIFY remainingAttemptsChanged)
    Q_PROPERTY(bool isWayland READ isWayland CONSTANT)

public:
    explicit SecurityQuestionHelper(const QJsonObject &payload, QObject *parent = nullptr);

    QVariantList questions() const { return m_questions; }
    bool isLock() const { return m_isLock; }
    QString userName() const { return m_userName; }
    QString fullName() const { return m_fullName; }
    bool locked() const { return m_locked; }
    QString unlockTime() const { return m_unlockTime; }
    int remainingAttempts() const { return m_remaining; }
    bool isWayland() const
    {
        return QGuiApplication::platformName().contains(QStringLiteral("wayland"), Qt::CaseInsensitive);
    }

    Q_INVOKABLE QString questionText(int index) const
    {
        return ::questionText(index);
    }

    // 提交答案：异步调用 DBus 验证，结果通过 verificationFinished 信号回传
    Q_INVOKABLE void submitAnswers(const QVariantMap &answers);

    // 异步重新查询 GetSqLimits，更新锁定状态/剩余次数后发出 verificationFinished(false,...)
    void refreshLimitsAsync();

    // 密码强度（0/1/2/3）
    Q_INVOKABLE int passwordLevel(const QString &pwd);

    // 密码规则校验，返回错误提示（空串表示通过）
    Q_INVOKABLE QString checkPassword(const QString &fullName,
                                      const QString &user,
                                      const QString &pwd);

    // 重置密码并设置提示；非锁屏场景终止 login1 会话。结果通过 resetFinished 信号回传
    Q_INVOKABLE void resetPassword(const QString &newPwd, const QString &hint);

    // Release only the X11 grabs. Window activation uses this path so that an
    // input method already being initialized for the current editor is not
    // hidden/reset while the window is acquiring its grabs.
    void releaseInputGrabs();

    // Process-exit fallback: release whichever window still owns or is
    // waiting for a grab and tear down the input-method context.
    Q_INVOKABLE void releaseAllInputGrabs();

    // 取消流程：使验证会话失效并退出
    Q_INVOKABLE void cancelFlow();

    // 结束流程：写 stdout JSON 并退出
    Q_INVOKABLE void finish(bool accepted, bool verified, bool passwordChanged);

    // 输入法：item 为 QML 侧首个答案输入框
    Q_INVOKABLE void setupIme(QQuickItem *item);

    Q_INVOKABLE void refreshIme(QQuickItem *item);

    Q_INVOKABLE void activateAndGrabInput(QObject *winObj);

Q_SIGNALS:
    void lockedChanged();
    void remainingAttemptsChanged();
    void finished();
    void inputGrabAcquired();
    // 异步验证完成：success=true 表示验证通过
    void verificationFinished(bool success, int remaining, bool locked, const QString &unlockTime);
    // 异步密码重置完成
    void resetFinished(bool success, const QString &error);

private:
    void scheduleGrab(QQuickWindow *win, quint64 generation, int attempt);

    ResetContext m_ctx;
    QVariantList m_questions;
    QPointer<QQuickWindow> m_grabWindow;
    QPointer<QQuickWindow> m_pendingGrabWindow;
    quint64 m_grabGeneration = 0;
    bool m_finished = false;
    bool m_isLock;
    int m_uid;
    QString m_userName;
    QString m_fullName;
    bool m_locked = false;
    QString m_unlockTime;
    int m_remaining = 0;
    InputMethodBackend m_imeBackend;
};

#endif // SECURITYQUESTIONHELPER_H
