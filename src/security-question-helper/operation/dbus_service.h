// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sq_helper_types.h"

#include <QDBusPendingCall>
#include <QPair>
#include <QString>

// 异步发起安全问题验证，返回 pending call，结果通过 watcher 获取
QDBusPendingCall verifySecretQuestions(const ResetContext &ctx);
// 异步发起密码重置，返回 pending call，结果通过 watcher 获取
QDBusPendingCall resetPasswordWithToken(const ResetContext &ctx, const QString &newPassword);
// 同步设置密码提示（流程收尾阶段调用，不阻塞交互）
void setPasswordHint(const ResetContext &ctx, const QString &hint);
void invalidateSession(const ResetContext &ctx);

// 同步查询限制（启动阶段防御性检查用）
SqLimitsInfo querySqLimits(const QString &dbusObjectPath);
// 异步查询限制，返回 pending call，结果通过 watcher 获取
QDBusPendingCall querySqLimitsAsync(const QString &dbusObjectPath);

QString cryptUserPassword(const QString &password);
