// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <DGuiApplicationHelper>
#include <DLog>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QTranslator>
#include <unistd.h>
#include "securityquestionhelper.h"

DCORE_USE_NAMESPACE
DGUI_USE_NAMESPACE

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // 解决 Qt 在 Retina 屏幕上图片模糊问题（需要在 QApplication 创建之前设置）
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    // The helper must always follow the system theme. Do not restore or persist
    // an application-specific theme for this short-lived process.
    DGuiApplicationHelper::setAttribute(DGuiApplicationHelper::DontSaveApplicationTheme, true);

    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName(QStringLiteral("deepin"));
    app.setApplicationName(QStringLiteral("dde-security-question-helper"));

    // DLogManager::registerConsoleAppender();
    DLogManager::registerJournalAppender();
    // 诊断用：helper 的 stdout/stderr 会被 dde-lock/greeter 消费，
    // journald 也未必捕获得到，因此额外落盘到固定文件便于远程抓取。
    QString logPath = QStringLiteral("/tmp/dde-security-question-helper-%1.log").arg(QString::number(getuid()));
    DLogManager::setlogFilePath(logPath);
    DLogManager::registerFileAppender();

    // 登录场景下 greeter 设置了 Thin 字重且无法读取 gsettings，默认 9pt 字体偏小
    QCommandLineParser parser;
    QCommandLineOption payloadOption(QStringList() << QStringLiteral("payload"), QStringLiteral("encoded payload"), QStringLiteral("payload"));
    parser.addOption(payloadOption);
    parser.process(app);

    const QByteArray payloadData = QByteArray::fromBase64(parser.value(payloadOption).toUtf8(), QByteArray::Base64UrlEncoding);
    QJsonParseError parseError;
    const QJsonDocument payloadDoc = QJsonDocument::fromJson(payloadData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !payloadDoc.isObject()) {
        return 1;
    }

    const QJsonObject payload = payloadDoc.object();
    const bool isLock = payload.value(QStringLiteral("isLock")).toBool();

    if (!isLock) {
        QFont appFont;
        appFont.setFamily(QStringLiteral("Noto Sans CJK SC"));
        appFont.setPointSize(10.5);
        appFont.setWeight(QFont::Normal);
        app.setFont(appFont);
    }

    // 加载翻译（复用主程序翻译文件，qml 字符串已由主 CMakeLists 的 lupdate 收集）
    const QStringList translateDirs = { TRANSLATE_READ_DIR,
                                        TRANSLATE_READ_DIR "/../v1.0", // 兼容旧版位置
                                        TRANSLATE_READ_DIR "/.." };
    DGuiApplicationHelper::loadTranslator(QStringLiteral("dde-control-center"), translateDirs, { QLocale() });

    auto *applicationHelper = DGuiApplicationHelper::instance();
    const auto applySystemPalette = [applicationHelper](DGuiApplicationHelper::ColorType themeType) {
        if (themeType == DGuiApplicationHelper::UnknownType) {
            return;
        }

        applicationHelper->setApplicationPalette(applicationHelper->applicationPalette(themeType));
    };
    applySystemPalette(applicationHelper->themeType());
    QObject::connect(applicationHelper, &DGuiApplicationHelper::themeTypeChanged,
                     &app, [applySystemPalette](DGuiApplicationHelper::ColorType themeType) {
        applySystemPalette(themeType);
    });

    SecurityQuestionHelper helper(payload);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("sqHelper"), &helper);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     &helper, &SecurityQuestionHelper::releaseAllInputGrabs);

    QObject::connect(&engine, &QQmlApplicationEngine::quit, &app, [] {
        qApp->exit();
    });

    QObject::connect(&helper, &SecurityQuestionHelper::finished, &app, [] {
        qApp->exit();
    });

    engine.load(QUrl(QStringLiteral("qrc:/security-question-helper/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window) {
        return 1;
    }

    QScreen *targetScreen = QGuiApplication::primaryScreen();
    if (targetScreen) {
        window->setScreen(targetScreen);
        const QRect screenGeometry = targetScreen->geometry();
        const QPoint centerPosition(
            screenGeometry.x() + (screenGeometry.width() - window->width()) / 2,
            screenGeometry.y() + (screenGeometry.height() - window->height()) / 2);
        window->setPosition(centerPosition);
    }
    window->show();

    const int exitCode = app.exec();
    return exitCode;
}
