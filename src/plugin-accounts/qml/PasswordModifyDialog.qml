// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Window
import QtQml.Models
import QtQuick.Layouts 1.15
import org.deepin.dtk 1.0 as D
import org.deepin.dcc 1.0

D.DialogWindow {
    id: dialog
    property string userId
    // 域账号修改密码时远端返回的结果信息（本地账号不显示）
    property string remoteTitle: ""
    property string remoteContent: ""
    property bool remoteRichText: false
    width: 460
    minimumWidth: width
    minimumHeight: height
    maximumWidth: minimumWidth
    maximumHeight: minimumHeight
    icon: "preferences-system"
    modality: Qt.WindowModal
    title: isCurrent() ? qsTr("Modify password") : qsTr("Reset password")

    function isCurrent() {
        return dialog.userId === dccData.currentUserId()
    }

    ColumnLayout {
        width: dialog.width - 10
        spacing: 0
        Label {
            text: {
                if (dialog.isCurrent())
                    return qsTr("Password length should be at least 8 characters, and the password should contain a combination of at least 3 of the following: uppercase letters, lowercase letters, numbers, and symbols. This type of password is more secure.")
                else
                    return qsTr("Resetting the password will clear the data stored in the keyring.")
            }
            font: D.DTK.fontManager.t8
            wrapMode: Text.WordWrap
            rightPadding: 10
            leftPadding: 10
            horizontalAlignment: Text.AlignHCenter
            Layout.preferredWidth: pwdLayout.minWidth(font, text, dialog.width - 12)
            Layout.leftMargin: 0
            Layout.rightMargin: 10
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        }

        PasswordLayout {
            id: pwdLayout
            userId: dialog.userId
            currentPwdVisible: dialog.isCurrent()
            Layout.leftMargin: 0
            Layout.rightMargin: 18
            Layout.fillWidth: true
            Layout.maximumWidth: dialog.width - 12
            onRequestClose: {
                // no error, close dialog
                close()
            }
            onRemoteResult: function (title, content, richText) {
                dialog.remoteTitle = title
                dialog.remoteContent = content
                dialog.remoteRichText = richText
                remoteErrorLoader.active = true
            }
        }

        RowLayout {
            spacing: 6
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
            Layout.bottomMargin: 6
            Layout.leftMargin: 0
            Layout.rightMargin: 16

            Button {
                id: cancelButton
                Layout.fillWidth: true
                text: qsTr("Cancel")
                font: D.DTK.fontManager.t7
                onClicked: {
                    close()
                }
            }
            D.RecommandButton {
                Layout.fillWidth: true
                text: qsTr("Modify password")
                font: D.DTK.fontManager.t7
                onClicked: {
                    if (!pwdLayout.checkPassword())
                    {
                        console.warn("----checkPassword failed")
                        return
                    }

                    dccData.setPassword(dialog.userId, pwdLayout.getPwdInfo());
                }
            }
        }
    }

    // 域账号远端返回结果弹框
    Loader {
        id: remoteErrorLoader
        active: false
        sourceComponent: DomainPasswordErrorDialog {
            title: dialog.remoteTitle
            message: dialog.remoteContent
            richText: dialog.remoteRichText
            onClosing: function () {
                remoteErrorLoader.active = false
            }
        }
        onLoaded: function () {
            remoteErrorLoader.item.show()
        }
    }
}
