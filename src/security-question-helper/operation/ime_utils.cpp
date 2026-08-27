// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ime_utils.h"

#include <QDebug>
#include <QGuiApplication>
#include <QInputMethod>
#include <QProcess>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

void refreshInputMethod(QQuickItem *item)
{
    if (!item) {
        qWarning() << "[SQ-DIAG-IME] refreshInputMethod: item is null";
        return;
    }

    qWarning() << "[SQ-DIAG-IME] refreshInputMethod: before forceActiveFocus"
               << "hasFocus=" << item->hasFocus()
               << "hasActiveFocus=" << item->hasActiveFocus()
               << "windowActive=" << (item->window() ? item->window()->isActive() : -1)
               << "visible=" << item->isVisible();

    item->forceActiveFocus();

    qWarning() << "[SQ-DIAG-IME] refreshInputMethod: after forceActiveFocus"
               << "hasFocus=" << item->hasFocus()
               << "hasActiveFocus=" << item->hasActiveFocus()
               << "windowActive=" << (item->window() ? item->window()->isActive() : -1);

    if (QInputMethod *inputMethod = QGuiApplication::inputMethod()) {
        inputMethod->update(Qt::ImEnabled | Qt::ImHints | Qt::ImQueryInput);
        inputMethod->update(Qt::ImQueryAll);
        inputMethod->show();
        // isVisible() reports the Qt input panel state. On X11, an external
        // fcitx candidate window can work while this remains false.
        qWarning() << "[SQ-DIAG-IME] refreshInputMethod: show requested, input panel visible="
                   << inputMethod->isVisible();
    } else {
        qWarning() << "[SQ-DIAG-IME] refreshInputMethod: QInputMethod is null";
    }
}

InputMethodBackend detectInputMethodBackend()
{
    auto buildBackend = [](const QString &name) {
        InputMethodBackend backend;
        backend.envName = name;
        if (name.isEmpty())
            return backend;

        backend.executable = QStandardPaths::findExecutable(name);
        const QString remoteExecutable = QStandardPaths::findExecutable(QStringLiteral("%1-remote").arg(name));
        backend.remoteCommand = remoteExecutable.isEmpty() ? QStringLiteral("%1-remote").arg(name) : remoteExecutable;
        return backend;
    };

    const QString qtImModule = qEnvironmentVariable("QT_IM_MODULE");
    if (!qtImModule.isEmpty() && qtImModule != QStringLiteral("xim")) {
        InputMethodBackend backend = buildBackend(qtImModule);
        if (!backend.executable.isEmpty())
            return backend;
    }

    for (const QString &candidate : { QStringLiteral("fcitx5"), QStringLiteral("fcitx") }) {
        InputMethodBackend backend = buildBackend(candidate);
        if (!backend.executable.isEmpty())
            return backend;
    }

    return buildBackend(qtImModule);
}

QString runRemoteCommand(const InputMethodBackend &backend, const QStringList &arguments)
{
    if (backend.remoteCommand.isEmpty())
        return QString();

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(backend.remoteCommand, arguments);
    if (!process.waitForStarted(1000))
        return QString();
    if (!process.waitForFinished(1500)) {
        process.kill();
        process.waitForFinished(500);
        return QStringLiteral("<timeout>");
    }
    if (process.exitStatus() != QProcess::NormalExit)
        return QStringLiteral("<crash>");

    return QString::fromLocal8Bit(process.readAllStandardOutput().trimmed());
}

QString readRemoteState(const InputMethodBackend &backend)
{
    return runRemoteCommand(backend, QStringList());
}

void ensureInputMethodActivated(const InputMethodBackend &backend, QQuickItem *item, const QString &stage)
{
    if (!item || !item->hasActiveFocus()) {
        qWarning() << "[SQ-DIAG-IME] ensureInputMethodActivated stage:" << stage
                   << "SKIP (item null or no active focus)"
                   << "item=" << item
                   << "hasFocus=" << (item ? item->hasFocus() : -1)
                   << "hasActiveFocus=" << (item ? item->hasActiveFocus() : -1)
                   << "windowActive=" << (item && item->window() ? item->window()->isActive() : -1);
        return;
    }

    qWarning() << "[SQ-DIAG-IME] ensureInputMethodActivated stage:" << stage
               << "hasActiveFocus=true, querying remote state...";

    refreshInputMethod(item);
    const QString stateBefore = readRemoteState(backend);
    qWarning() << "[SQ-DIAG-IME] ensureInputMethodActivated stage:" << stage
               << "fcitx stateBefore='<< stateBefore << \"'";

    if (stateBefore == QStringLiteral("0") || stateBefore == QStringLiteral("1")) {
        const QString out = runRemoteCommand(backend, QStringList() << QStringLiteral("-o"));
        const QString stateAfter = readRemoteState(backend);
        qWarning() << "[SQ-DIAG-IME] ensureInputMethodActivated stage:" << stage
                   << "sent -o, output='" << out << "' stateAfter='" << stateAfter << "'";
        return;
    }

    qInfo() << "[SQ-Helper] IME state stage:" << stage << "state:" << stateBefore;
}

void setupImeForItem(const InputMethodBackend &backend, QQuickItem *item)
{
    if (!item)
        return;

    QTimer::singleShot(0, item, [item] {
        refreshInputMethod(item);
        QTimer::singleShot(50, item, [item] {
            refreshInputMethod(item);
        });
    });

    for (const int delay : { 300, 800, 1500 }) {
        QTimer::singleShot(delay, item, [backend, item, delay] {
            ensureInputMethodActivated(backend, item, QStringLiteral("delayed-%1ms").arg(delay));
        });
    }
}
