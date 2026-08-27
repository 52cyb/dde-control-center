// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sq_helper_types.h"

#include <QQuickItem>

void refreshInputMethod(QQuickItem *item);
InputMethodBackend detectInputMethodBackend();
QString runRemoteCommand(const InputMethodBackend &backend, const QStringList &arguments);
QString readRemoteState(const InputMethodBackend &backend);
void ensureInputMethodActivated(const InputMethodBackend &backend, QQuickItem *item, const QString &stage);
void setupImeForItem(const InputMethodBackend &backend, QQuickItem *item);
