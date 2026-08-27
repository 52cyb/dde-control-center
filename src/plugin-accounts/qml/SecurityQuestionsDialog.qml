// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Window
import QtQml.Models
import QtQuick.Layouts 1.15
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS
import org.deepin.dcc 1.0

D.DialogWindow {
    id: dialog
    property string userId
    width: 460
    minimumWidth: width
    minimumHeight: height
    maximumWidth: minimumWidth
    maximumHeight: minimumHeight
    icon: "preferences-system"
    modality: Qt.WindowModal
    // title: qsTr("Security Questions Password Reset")

    ColumnLayout {
        width: dialog.width - 10
        spacing: 0

        Connections {
            target: dccData
            function onSecurityQuestionsSetFinished(error) {
                if (error.length > 0) {
                    console.warn("Set security questions failed:", error)
                    // errorTip.visible = true
                    // errorTip.text = error
                    saveButton.enabled = true
                } else {
                    // 与 gerrit 一致：设置成功后重新查询已设问题，刷新 UI 状态
                    dccData.asyncSecurityQuestionsCheck(dialog.userId)
                    dialog.close()
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.alignment: Qt.AlignHCenter
            font.bold: true
            font.pointSize: 14
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Security Questions Password Reset")
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
            text: qsTr("If you forget your password, you can reset it via the following security questions")
        }

        // Label {
        //     id: errorTip
        //     visible: false
        //     Layout.fillWidth: true
        //     Layout.leftMargin: 10
        //     Layout.rightMargin: 10
        //     Layout.topMargin: 4
        //     Layout.alignment: Qt.AlignHCenter
        //     color: "#FF5736"
        //     font: D.DTK.fontManager.t8
        //     horizontalAlignment: Text.AlignHCenter
        //     wrapMode: Text.WordWrap
        // }

        SecurityQuestionsLayout {
            id: sqLayout
            required: true
            Layout.topMargin: 16
            Layout.leftMargin: 12 - DS.Style.dialogWindow.contentHMargin
            Layout.rightMargin: 6
        }

        RowLayout {
            spacing: 6
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
            Layout.bottomMargin: 6
            Layout.leftMargin: 6 - DS.Style.dialogWindow.contentHMargin
            Layout.rightMargin: 6
            Layout.topMargin: 60

            Button {
                Layout.fillWidth: true
                Layout.preferredWidth: 180
                text: qsTr("Cancel")
                font: D.DTK.fontManager.t7
                onClicked: {
                    dialog.close()
                }
            }
            D.RecommandButton {
                id: saveButton
                Layout.fillWidth: true
                Layout.preferredWidth: 180
                text: qsTr("Save")
                font: D.DTK.fontManager.t7
                onClicked: {
                    if (!sqLayout.validate())
                        return
                    saveButton.enabled = false
                    dccData.setSecurityQuestions(dialog.userId, sqLayout.collectQuestions())
                }
            }
        }
    }
}
