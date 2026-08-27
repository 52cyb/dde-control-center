// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

Item {
    id: verifyPage

    signal verified()
    signal cancelled()

    anchors.fill: parent
    implicitHeight: contentLayout.implicitHeight

    property var answerEdits: []
    property bool locked: sqHelper.locked
    property string unlockTime: sqHelper.unlockTime
    property int labelWidth: 40

    function setInputsEnabled(enabled) {
        for (var i = 0; i < answerEdits.length; i++) {
            answerEdits[i].enabled = enabled
            answerEdits[i].readOnly = !enabled
        }
        confirmButton.enabled = enabled
    }

    function activateInput() {
        if (verifyPage.locked || answerEdits.length === 0)
            return

        answerEdits[0].forceActiveFocus()
        sqHelper.setupIme(answerEdits[0])
    }

    function refreshInput() {
        if (verifyPage.locked || answerEdits.length === 0)
            return

        sqHelper.refreshIme(answerEdits[0])
    }

    function showError(errorMsg) {
        statusLabel.text = errorMsg
        statusLabel.visible = errorMsg.length > 0
    }

    function clearInputs() {
        for (var i = 0; i < answerEdits.length; i++) {
            answerEdits[i].text = ""
            answerEdits[i].showAlert = false
            answerEdits[i].alertText = ""
        }
    }

    function updateRemainingAttempts(remaining) {
        if (remaining > 0) {
            statusLabel.text = qsTr("Remaining attempts: %1").arg(remaining)
            statusLabel.visible = true
        } else {
            statusLabel.visible = false
        }
    }

    function setLockedState() {
        if (verifyPage.unlockTime.length === 0)
            return
        verifyPage.locked = true
        setInputsEnabled(false)
        countdownTimer.start()
        updateLockCountdown()
    }

    function updateLockCountdown() {
        var unlockDate = Date.fromString ? new Date(verifyPage.unlockTime) : new Date(verifyPage.unlockTime)
        var remainingSecs = (unlockDate.getTime() - Date.now()) / 1000
        if (remainingSecs <= 0) {
            verifyPage.locked = false
            statusLabel.visible = false
            countdownTimer.stop()
            setInputsEnabled(true)
            if (answerEdits.length > 0)
                answerEdits[0].forceActiveFocus()
            return
        }

        var remainingMinutes = Math.ceil(remainingSecs / 60)
        statusLabel.text = qsTr("Too many failed attempts. Please try again in %1 minutes.").arg(remainingMinutes)
        statusLabel.visible = true
    }

    function collectAnswers() {
        var answers = {}
        for (var i = 0; i < sqHelper.questions.length; i++) {
            var edit = answerEdits[i]
            if (!edit) {
                console.warn("[SQ-Helper] collectAnswers: answerEdits[" + i + "] is null, questions.length=" + sqHelper.questions.length + ", answerEdits.length=" + answerEdits.length)
                continue
            }
            answers[String(sqHelper.questions[i])] = edit.text.trim()
        }
        return answers
    }

    function onConfirmClicked() {
        console.warn("[SQ-Helper] onConfirmClicked: questions.length=" + sqHelper.questions.length + ", answerEdits.length=" + answerEdits.length)
        var hasEmpty = false
        var firstEmptyIndex = -1
        for (var i = 0; i < answerEdits.length; i++) {
            answerEdits[i].showAlert = false
            answerEdits[i].alertText = ""
            if (answerEdits[i].text.trim().length === 0) {
                console.warn("[SQ-Helper] onConfirmClicked: answer " + i + " is empty")
                answerEdits[i].showAlert = true
                answerEdits[i].alertText = qsTr("Content cannot be empty")
                hasEmpty = true
                if (firstEmptyIndex < 0)
                    firstEmptyIndex = i
            }
        }

        if (firstEmptyIndex >= 0) {
            answerEdits[firstEmptyIndex].forceActiveFocus()
            return
        }

        if (hasEmpty)
            return

        // 异步提交答案：验证结果通过 sqHelper.verificationFinished 信号回传
        console.warn("[SQ-Helper] onConfirmClicked: submitting answers=" + JSON.stringify(collectAnswers()))
        sqHelper.submitAnswers(collectAnswers())
    }

    Component.onCompleted: {
        if (sqHelper.locked && sqHelper.unlockTime.length > 0) {
            verifyPage.unlockTime = sqHelper.unlockTime
            verifyPage.setLockedState()
        }
    }

    ColumnLayout {
        id: contentLayout
        width: verifyPage.width
        spacing: 0

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.alignment: Qt.AlignHCenter
            font.bold: true
            font.pointSize: 14
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Reset Password")
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.topMargin: 2
            Layout.alignment: Qt.AlignHCenter
            font: D.DTK.fontManager.t8
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Please answer the following security questions to reset your password")
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 6
            Layout.topMargin: 16
            spacing: 0

            Repeater {
                id: questionRepeater
                model: sqHelper.questions

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: index === 0 ? 0 : 16
                    spacing: 0

                    // 问题行：标签 + 只读输入框
                    RowLayout {
                        spacing: 10
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("question%1").arg(index + 1)
                            Layout.preferredWidth: verifyPage.labelWidth
                            font: D.DTK.fontManager.t7
                        }

                        D.LineEdit {
                            id: questionEdit
                            Layout.fillWidth: true
                            implicitHeight: 30
                            leftPadding: 8
                            topPadding: 0
                            bottomPadding: 0
                            verticalAlignment: TextInput.AlignVCenter
                            text: sqHelper.questionText(modelData)
                            readOnly: true
                            focusPolicy: Qt.NoFocus
                            activeFocusOnTab: false
                            selectByMouse: false
                            clearButton.active: false
                            clearButton.visible: false
                            font: D.DTK.fontManager.t7

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onPressed: mouse => mouse.accepted = true
                            }
                        }
                    }

                    // 答案行：标签 + 输入框
                    RowLayout {
                        spacing: 10
                        Layout.fillWidth: true
                        Layout.topMargin: 8

                        Label {
                            text: qsTr("answer%1").arg(index + 1)
                            Layout.preferredWidth: verifyPage.labelWidth
                            font: D.DTK.fontManager.t7
                        }

                        D.LineEdit {
                            id: answerEdit
                            Layout.fillWidth: true
                            implicitHeight: 30
                            leftPadding: 8
                            topPadding: 0
                            bottomPadding: 0
                            verticalAlignment: TextInput.AlignVCenter
                            placeholderText: qsTr("Required")
                            alertDuration: 2000
                            echoMode: TextInput.Normal
                            font: D.DTK.fontManager.t7
                            onTextChanged: {
                                if (showAlert)
                                    showAlert = false
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onPressed: mouse => mouse.accepted = true
                            }

                            Component.onCompleted: {
                                answerEdits.push(answerEdit)
                            }
                        }
                    }
                }
            }
        }

        D.Label {
            id: statusLabel
            visible: false
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.topMargin: 40
            Layout.alignment: Qt.AlignHCenter
            font: D.DTK.fontManager.t8
            color: D.DTK.themeType === D.ApplicationHelper.DarkType ? "#FF5736" : "#E6452B"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        FontMetrics {
            id: sqFm
            font: D.DTK.fontManager.t7
            Component.onCompleted: {
                var w1 = sqFm.advanceWidth(qsTr("question%1").arg(1))
                var w2 = sqFm.advanceWidth(qsTr("answer%1").arg(1))
                verifyPage.labelWidth = Math.max(w1, w2)
            }
        }

        Timer {
            id: countdownTimer
            interval: 30 * 1000
            repeat: true
            onTriggered: verifyPage.updateLockCountdown()
        }

        Connections {
            target: sqHelper
            function onVerificationFinished(success, remaining, locked, unlockTime) {
                console.warn("[SQ-Helper] verificationFinished: success=" + success
                             + ", remaining=" + remaining + ", locked=" + locked
                             + ", unlockTime=" + unlockTime)
                if (success) {
                    verifyPage.verified()
                    return
                }

                // 验证失败：清空输入并根据最新限制状态展示
                verifyPage.clearInputs()
                if (locked && unlockTime.length > 0) {
                    verifyPage.unlockTime = unlockTime
                    verifyPage.setLockedState()
                } else if (remaining <= 0) {
                    verifyPage.showError(qsTr("Too many failed attempts. Please try again later."))
                } else {
                    verifyPage.updateRemainingAttempts(remaining)
                    verifyPage.showError(qsTr("One or more answers are incorrect. You can try %1 more times.").arg(remaining))
                }
            }
        }

        RowLayout {
            spacing: 6
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
            Layout.bottomMargin: 6
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            Layout.topMargin: statusLabel.visible ? 8 : 65

            Button {
                Layout.fillWidth: true
                Layout.preferredWidth: 180
                text: qsTr("Cancel")
                font: D.DTK.fontManager.t7
                onClicked: {
                    verifyPage.cancelled()
                }
            }
            D.RecommandButton {
                id: confirmButton
                Layout.fillWidth: true
                Layout.preferredWidth: 180
                text: qsTr("Confirm")
                font: D.DTK.fontManager.t7
                onClicked: verifyPage.onConfirmClicked()
            }
        }
    }
}
