// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D

// 域账号修改密码时，展示远端返回的结果信息，支持换行完整展示
D.DialogWindow {
    id: dialog
    property string message: ""
    property bool richText: false
    width: 600
    minimumWidth: width
    maximumWidth: minimumWidth
    icon: "dialog-warning"
    modality: Qt.WindowModal

    ColumnLayout {
        Label {
            Layout.alignment: Qt.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font: D.DTK.fontManager.t5
            text: dialog.message
            wrapMode: Text.WordWrap
            textFormat: dialog.richText ? Text.RichText : Text.PlainText
            Layout.preferredWidth: dialog.width - 10
            Layout.topMargin: 10
            leftPadding: 10
            rightPadding: 10
        }

        Button {
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.topMargin: 10
            Layout.bottomMargin: 10
            Layout.fillWidth: true
            text: qsTr("Confirm")
            onClicked: {
                dialog.close()
            }
        }
    }
}
