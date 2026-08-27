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

ColumnLayout {
    id: sqLayout
    property bool required: true
    Layout.fillWidth: true
    spacing: 0

    readonly property int pairCount: 3
    readonly property int maxAnswerLength: 30
    property int labelWidth: -1
    property var questionTexts: [
        qsTr("What is the name of the city where you were born?"),
        qsTr("What is the name of your alma mater?"),
        qsTr("Who is the person you love the most?"),
        qsTr("What is your favorite animal?"),
        qsTr("What is your favorite music?"),
        qsTr("What is your nickname?")
    ]

    function placeholderText(seq) {
        return qsTr("Please select question%1").arg(seq)
    }

    // 同步各下拉框可用状态：首项(占位)永远禁用，已被其他下拉框选中的问题禁用
    function updateEnabledStates() {
        var selected = []
        for (var i = 0; i < pairsRepeater.count; i++) {
            var item = pairsRepeater.itemAt(i)
            if (!item || !item.combo)
                continue
            var idx = item.combo.currentIndex
            if (idx > 0)
                selected.push(idx)
        }
        for (var j = 0; j < pairsRepeater.count; j++) {
            var item2 = pairsRepeater.itemAt(j)
            if (!item2 || !item2.combo)
                continue
            var combo = item2.combo
            var mdl = combo.model
            for (var row = 0; row < mdl.count; row++) {
                var disabledByOther = (row > 0) && (selected.indexOf(row) >= 0) && (combo.currentIndex !== row)
                mdl.setProperty(row, "enabled", row > 0 && !disabledByOther)
            }
        }
    }

    function validate() {
        if (!sqLayout.required)
            return true
        var ok = true
        for (var i = 0; i < pairsRepeater.count; i++) {
            var pair = pairsRepeater.itemAt(i)
            if (!pair)
                continue
            var edit = pair.edit
            edit.showAlert = false
            if (pair.combo.currentIndex === 0) {
                edit.showAlert = true
                edit.alertText = qsTr("Please select a security question first")
                ok = false
            } else if (edit.text.length === 0) {
                edit.showAlert = true
                edit.alertText = qsTr("This field is required")
                ok = false
            }
        }
        return ok
    }

    function collectQuestions() {
        var list = []
        for (var i = 0; i < pairsRepeater.count; i++) {
            var pair = pairsRepeater.itemAt(i)
            if (!pair)
                continue
            list.push({
                "id": pair.combo.currentIndex,
                "encryptedAnswer": dccData.encryptSecurityAnswer(pair.edit.text)
            })
        }
        return list
    }

    Repeater {
        id: pairsRepeater
        model: sqLayout.pairCount

        ColumnLayout {
            id: pair
            property alias combo: questionCombo
            property alias edit: answerEdit
            property int pairIndex: index
            property int labelWidth: 40
            Layout.fillWidth: true
            Layout.topMargin: index === 0 ? 0 : 16
            spacing: 0

            FontMetrics {
                id: sqFm
                font: D.DTK.fontManager.t7
                Component.onCompleted: {
                    if (sqLayout.labelWidth < 0) {
                        var w1 = sqFm.advanceWidth(qsTr("question%1").arg(pair.pairIndex + 1))
                        var w2 = sqFm.advanceWidth(qsTr("answer%1").arg(pair.pairIndex + 1))
                        pair.labelWidth = Math.max(w1, w2)
                    }
                }
            }

            RowLayout {
                spacing: 10
                Layout.fillWidth: true

                Label {
                    text: qsTr("question%1").arg(pair.pairIndex + 1)
                    Layout.preferredWidth: sqLayout.labelWidth >= 0 ? sqLayout.labelWidth : pair.labelWidth
                    font: D.DTK.fontManager.t7
                }

                ComboBox {
                    id: questionCombo
                    Layout.fillWidth: true
                    implicitHeight: 30
                    font: D.DTK.fontManager.t7
                    model: ListModel {
                        id: questionModel
                        Component.onCompleted: {
                            append({ "text": sqLayout.placeholderText(pair.pairIndex + 1), "enabled": false })
                            for (var i = 0; i < sqLayout.questionTexts.length; i++)
                                append({ "text": sqLayout.questionTexts[i], "enabled": true })
                        }
                    }
                    contentItem: Text {
                        leftPadding: 0
                        rightPadding: questionCombo.indicator.width + questionCombo.spacing
                        text: {
                            if (questionCombo.currentIndex === 0)
                                return sqLayout.placeholderText(pair.pairIndex + 1)
                            var item = questionModel.get(questionCombo.currentIndex)
                            return item ? item.text : ""
                        }
                        color: questionCombo.currentIndex === 0 ? questionCombo.palette.placeholderText : questionCombo.palette.text
                        font: questionCombo.font
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    delegate: MenuItem {
                        required property var model
                        required property int index
                        width: ListView.view.width
                        useIndicatorPadding: true
                        text: model.text
                        font: questionCombo.font
                        enabled: model.enabled
                        highlighted: questionCombo.highlightedIndex === index
                        hoverEnabled: questionCombo.hoverEnabled
                        autoExclusive: true
                        checked: questionCombo.currentIndex === index
                    }
                    onCurrentIndexChanged: {
                        answerEdit.clear()
                        sqLayout.updateEnabledStates()
                    }
                    Component.onCompleted: {
                        currentIndex = 0
                    }
                }
            }

            RowLayout {
                spacing: 10
                Layout.fillWidth: true
                Layout.topMargin: 8

                Label {
                    text: qsTr("answer%1").arg(pair.pairIndex + 1)
                    Layout.preferredWidth: sqLayout.labelWidth >= 0 ? sqLayout.labelWidth : pair.labelWidth
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
                    font: D.DTK.fontManager.t7
                    alertDuration: 3000

                    property string lastValidAnswer: ""
                    property bool restoringAnswer: false

                    onTextChanged: {
                        if (restoringAnswer) {
                            restoringAnswer = false
                            return
                        }
                        if (text.indexOf(" ") >= 0) {
                            var filtered = text.replace(/ /g, "")
                            text = filtered
                        }
                        if (showAlert)
                            showAlert = false

                        if (text.length > sqLayout.maxAnswerLength) {
                            showAlert = true
                            alertText = qsTr("Answer cannot exceed 30 characters")
                            restoringAnswer = true
                            text = lastValidAnswer
                        } else {
                            lastValidAnswer = text
                        }
                    }
                }
            }
        }
    }
}
