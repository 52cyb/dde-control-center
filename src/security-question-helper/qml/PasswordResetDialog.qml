// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

Item {
    id: resetPage

    signal success()
    signal cancelled()

    anchors.fill: parent
    implicitHeight: contentLayout.implicitHeight

    property string password: ""
    property string repeatPassword: ""
    property string passwordHint: ""

    function showError(msg) {
        statusLabel.text = msg
        statusLabel.visible = msg.length > 0
    }

    function focusAndSelect(edit) {
        edit.forceActiveFocus()
        edit.selectAll()
    }

    function activateInput() {
        passwordEdit.forceActiveFocus()
        sqHelper.setupIme(passwordEdit)
    }

    function refreshInput() {
        sqHelper.refreshIme(passwordEdit)
    }

    function onConfirmClicked() {
        const err = sqHelper.checkPassword(sqHelper.fullName, sqHelper.userName, password)
        if (err.length > 0) {
            passwordEdit.showAlert = true
            passwordEdit.alertText = err
            focusAndSelect(passwordEdit)
            return
        }

        if (repeatPassword.length < 1) {
            repeatEdit.showAlert = true
            repeatEdit.alertText = qsTr("Password cannot be empty")
            focusAndSelect(repeatEdit)
            return
        }

        if (repeatPassword !== password) {
            repeatEdit.showAlert = true
            repeatEdit.alertText = qsTr("Passwords do not match")
            focusAndSelect(repeatEdit)
            return
        }

        if (passwordHint.length > 0) {
            for (const ch of password) {
                if (passwordHint.includes(ch)) {
                    hintEdit.showAlert = true
                    hintEdit.alertText = qsTr("The hint is visible to all users. Do not include the password here.")
                    focusAndSelect(hintEdit)
                    return
                }
            }
        }

        // 异步重置密码：结果通过 sqHelper.resetFinished 信号回传
        sqHelper.resetPassword(password, passwordHint)
    }

    function updateStrengthIndicator(level) {
        var colors = [ "#FF5736", "#FFAA00", "#15BB18" ]
        var activeColor = "transparent"
        if (level > 0)
            activeColor = colors[level - 1]
        for (var j = 0; j < strengthBars.count; j++) {
            strengthBars.itemAt(j).color = (j < level) ? activeColor : strengthBars.itemAt(j).defaultColor
        }
    }

    ColumnLayout {
        id: contentLayout
        width: resetPage.width
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
            Layout.topMargin: 8
            Layout.alignment: Qt.AlignHCenter
            font: D.DTK.fontManager.t8
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Resetting the password will clear the data stored in the keyring.")
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.topMargin: 16
            spacing: 10

            D.PasswordEdit {
                id: passwordEdit
                Layout.fillWidth: true
                implicitHeight: 32
                topPadding: 0
                bottomPadding: 0
                verticalAlignment: TextInput.AlignVCenter
                placeholderText: qsTr("New password")
                alertDuration: 3000
                echoButtonVisible: true
                canCopy: false
                canCut: false
                inputMethodHints: Qt.ImhLatinOnly | Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                property color normalHighlight: "transparent"
                onTextChanged: {
                    password = text
                    if (showAlert)
                        showAlert = false
                    resetPage.updateStrengthIndicator(sqHelper.passwordLevel(text))
                }
                Component.onCompleted: {
                    normalHighlight = palette.highlight
                    palette.highlight = Qt.binding(function() {
                        return showAlert ? "#FF5736" : normalHighlight
                    })
                }
            }

            D.PasswordEdit {
                id: repeatEdit
                Layout.fillWidth: true
                implicitHeight: 32
                topPadding: 0
                bottomPadding: 0
                verticalAlignment: TextInput.AlignVCenter
                placeholderText: qsTr("Repeat Password")
                alertDuration: 3000
                echoButtonVisible: true
                canCopy: false
                canCut: false
                inputMethodHints: Qt.ImhLatinOnly | Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                property color normalHighlight: "transparent"
                onTextChanged: {
                    repeatPassword = text
                    if (showAlert)
                        showAlert = false
                }
                Component.onCompleted: {
                    normalHighlight = palette.highlight
                    palette.highlight = Qt.binding(function() {
                        return showAlert ? "#FF5736" : normalHighlight
                    })
                }
            }

            D.LineEdit {
                id: hintEdit
                Layout.fillWidth: true
                implicitHeight: 32
                topPadding: 0
                bottomPadding: 0
                verticalAlignment: TextInput.AlignVCenter
                placeholderText: qsTr("Password hint")
                alertDuration: 3000
                maximumLength: 14
                onTextChanged: {
                    passwordHint = text
                    if (showAlert)
                        showAlert = false
                }
            }
        }

        RowLayout {
            id: strengthRow
            spacing: 4
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            Layout.rightMargin: 16
            Layout.topMargin: 12
            Layout.bottomMargin: 4

            Repeater {
                id: strengthBars
                model: 3
                delegate: Rectangle {
                    property color defaultColor: palette.button
                    implicitHeight: 4
                    height: 4
                    width: 10
                    radius: 2
                    color: defaultColor
                }
            }
        }

        D.Label {
            id: statusLabel
            visible: false
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.alignment: Qt.AlignHCenter
            font: D.DTK.fontManager.t8
            color: D.DTK.themeType === D.ApplicationHelper.DarkType ? "#FF5736" : "#E6452B"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Connections {
            target: sqHelper
            function onResetFinished(success, error) {
                if (success) {
                    resetPage.success()
                } else {
                    showError(error.length > 0 ? error : qsTr("Failed to reset password, please try again."))
                }
            }
        }

        RowLayout {
            spacing: 6
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
            Layout.bottomMargin: 6
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            Layout.topMargin: 40

            Button {
                Layout.fillWidth: true
                Layout.preferredWidth: 180
                text: qsTr("Cancel")
                font: D.DTK.fontManager.t7
                onClicked: {
                    resetPage.cancelled()
                }
            }
            D.RecommandButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 180
                text: qsTr("Reset Password")
                font: D.DTK.fontManager.t7
                onClicked: resetPage.onConfirmClicked()
            }
        }
    }
}
