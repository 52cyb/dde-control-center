// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Window 2.15
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

D.DialogWindow {
    id: dialog

    palette: D.DTK.palette
    D.DWindow.enabled: true

    property bool passwordResetPage: false
    property bool closingFlow: false
    property bool switchingPage: false
    readonly property real preferredHeight: pageLoader.item
                                            ? pageLoader.item.implicitHeight
                                              + DS.Style.dialogWindow.titleBarHeight
                                              + DS.Style.dialogWindow.contentHMargin
                                            : 0

    property var inputGrabConnection: Connections {
        target: sqHelper

        function onInputGrabAcquired() {
            // Run after the grab callback has unwound, when the DialogWindow is
            // active and the current page has a valid window/input context.
            Qt.callLater(dialog.refreshCurrentPageInput)
        }
    }
    property Component verificationPage: Component {
        SecurityQuestionVerificationDialog {
            onVerified: {
                if (dialog.closingFlow || dialog.switchingPage)
                    return

                dialog.switchingPage = true
                dialog.passwordResetPage = true
                Qt.callLater(function() {
                    if (dialog.closingFlow) {
                        dialog.switchingPage = false
                        return
                    }

                    pageLoader.sourceComponent = dialog.resetPage
                    dialog.switchingPage = false
                })
            }
            onCancelled: dialog.cancelCurrentFlow()
        }
    }
    property Component resetPage: Component {
        PasswordResetDialog {
            onSuccess: sqHelper.finish(true, true, true)
            onCancelled: dialog.cancelCurrentFlow()
        }
    }

    function cancelCurrentFlow() {
        if (closingFlow)
            return

        closingFlow = true
        const invalidateSession = passwordResetPage
        sqHelper.releaseAllInputGrabs()
        pageLoader.sourceComponent = null
        Qt.callLater(function() {
            if (invalidateSession)
                sqHelper.cancelFlow()
            else
                sqHelper.finish(false, false, false)
        })
    }

    function activateCurrentPageInput() {
        if (closingFlow || pageLoader.status !== Loader.Ready || !pageLoader.item)
            return

        pageLoader.item.activateInput()
    }

    function refreshCurrentPageInput() {
        if (closingFlow || pageLoader.status !== Loader.Ready || !pageLoader.item)
            return

        pageLoader.item.refreshInput()
    }

    width: 460
    height: preferredHeight
    minimumWidth: 460
    minimumHeight: preferredHeight
    maximumWidth: 460
    maximumHeight: preferredHeight
    icon: "dialog-warning"
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.WindowStaysOnTopHint | Qt.WindowCloseButtonHint
           | Qt.MSWindowsFixedSizeDialogHint
           | (sqHelper.isWayland ? 0 : Qt.X11BypassWindowManagerHint)

    Loader {
        id: pageLoader
        anchors.fill: parent
        sourceComponent: dialog.verificationPage

        onLoaded: {
            if (!pageLoader.item)
                return

            pageLoader.item.visible = true
            Qt.callLater(dialog.activateCurrentPageInput)
        }
    }

    onVisibleChanged: {
        if (visible) {
            sqHelper.activateAndGrabInput(dialog)
        }
    }

    onClosing: function(close) {
        close.accepted = true
        dialog.cancelCurrentFlow()
    }
}
