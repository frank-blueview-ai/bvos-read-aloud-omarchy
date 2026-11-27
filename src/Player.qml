import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: card
    title: qsTr("BvOs Intelligence Read Aloud")
    width: 242
    height: compactHeight
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    property bool expanded: false
    property bool editingPreview: false
    readonly property bool fullscreen: visibility === Window.FullScreen
    readonly property int compactHeight: expanded ? (reader.hasPreview ? 203 : 67) : 32
    readonly property int surfaceHeight: fullscreen ? height : compactHeight
    readonly property int cornerRadius: fullscreen ? 0 : Math.min(reader.rounding, Math.floor(surfaceHeight / 2))
    onCompactHeightChanged: {
        updateInputArea()
        if (visible && !fullscreen)
            reader.resizeCard(242, compactHeight)
    }
    onCornerRadiusChanged: updateInputArea()
    onSurfaceHeightChanged: updateInputArea()
    onWidthChanged: if (fullscreen) updateInputArea()
    onFullscreenChanged: {
        collapse.stop()
        if (!fullscreen && !hover.hovered)
            collapse.restart()
        Qt.callLater(passagePreview.followHighlight)
    }
    onVisibleChanged: updateInputArea()

    function updateInputArea() {
        if (visible)
            reader.setCardInputArea(card, surfaceHeight, cornerRadius)
    }
    readonly property int passageSideMargin: fullscreen ? Math.max(24, Math.floor((width - 840) / 2)) : 10
    readonly property color ink: reader.dark ? "#eef3fb" : "#0f2a44"
    readonly property color muted: reader.dark ? "#b6c3d4" : "#4a6079"
    readonly property color subtle: reader.dark ? "#a3b1c3" : "#5a6f86"
    readonly property color accent: reader.dark ? "#8cc4ff" : "#2861b8"


    Timer {
        id: collapse
        interval: 140
        onTriggered: if (!card.fullscreen && !hover.hovered && !headerDrag.active) card.expanded = false
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (card.fullscreen)
                reader.setCardFullscreen(false)
            else
                card.close()
        }
    }

    Connections {
        target: reader
        function onShowCard() { if (!card.visible) card.show() }
    }

    component CardTip: ToolTip {
        id: tip
        delay: 650
        timeout: 3200
        padding: 6
        width: Math.min(implicitWidth, 224)
        popupType: Popup.Window

        contentItem: Text {
            text: tip.text
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: card.muted
            font.pixelSize: 11
        }
        background: Rectangle {
            radius: Math.min(card.cornerRadius, 8)
            color: reader.dark ? "#172131" : "#f4f7fb"
            border.color: reader.dark ? "#29374b" : "#dbe3ee"
        }
    }

    component PlayerButton: Button {
        id: control
        required property string label
        required property string symbol
        property bool primary: false
        Accessible.name: label
        CardTip { visible: control.hovered; text: control.label }
        Layout.preferredWidth: 28
        Layout.preferredHeight: 28
        padding: 0
        focusPolicy: Qt.NoFocus
        background: Rectangle {
            radius: width / 2
            color: Qt.alpha(card.accent, control.down ? 0.25
                : control.hovered && control.enabled ? 0.16
                : control.primary && control.enabled ? 0.10 : 0)
        }
        contentItem: Item {
            Canvas {
                id: glyph
                anchors.centerIn: parent
                width: 16
                height: 16
                scale: 0.85
                readonly property string shape: control.symbol
                readonly property int passageSideMargin: fullscreen ? Math.max(24, Math.floor((width - 840) / 2)) : 10
    readonly property color ink: !control.enabled ? Qt.alpha(card.subtle, 0.48)
                    : control.primary ? card.accent : card.ink
                onShapeChanged: requestPaint()
                onInkChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = ink
                    if (shape === "pause") {
                        ctx.fillRect(3, 2, 3, 12)
                        ctx.fillRect(10, 2, 3, 12)
                    } else if (shape === "fullscreen" || shape === "restore") {
                        const inset = shape === "restore" ? 5 : 2
                        const direction = shape === "restore" ? -1 : 1
                        for (const x of [-1, 1]) {
                            for (const y of [-1, 1]) {
                                const cornerX = x < 0 ? inset : 16 - inset
                                const cornerY = y < 0 ? inset : 16 - inset
                                ctx.fillRect(Math.min(cornerX, cornerX - x * direction * 4), cornerY - 0.75, 4, 1.5)
                                ctx.fillRect(cornerX - 0.75, Math.min(cornerY, cornerY - y * direction * 4), 1.5, 4)
                            }
                        }
                    } else if (shape === "stop") {
                        ctx.fillRect(3, 3, 10, 10)
                    } else {
                        const previous = shape === "previous"
                        const start = previous ? 13 : (shape === "play" ? 4 : 2)
                        const tip = previous ? 4 : (shape === "play" ? 14 : 11)
                        ctx.beginPath()
                        ctx.moveTo(start, 2)
                        ctx.lineTo(tip, 8)
                        ctx.lineTo(start, 14)
                        ctx.closePath()
                        ctx.fill()
                        if (shape !== "play")
                            ctx.fillRect(previous ? 1 : 13, 2, 2, 12)
                    }
                }
            }
        }
    }

    Item {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: card.surfaceHeight

        HoverHandler {
            id: hover
            onHoveredChanged: {
                if (hovered) {
                    collapse.stop()
                    card.expanded = true
                } else if (!card.fullscreen && !headerDrag.active) {
                    collapse.restart()
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            color: reader.dark ? "#0f1724" : "#ffffff"
            radius: card.cornerRadius
            antialiasing: radius > 0
        }

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 32
            spacing: 4

            Item { Layout.preferredWidth: 2 }

            Button {
                id: brandButton
                Layout.preferredWidth: 23
                Layout.preferredHeight: 23
                padding: 0
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Visit Blue View")
                CardTip { visible: brandButton.hovered; text: brandButton.Accessible.name }
                background: null
                contentItem: Image {
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/double-oval.svg"
                }
                onClicked: Qt.openUrlExternally("https://blueview.ai")
            }
            Text {
                text: "BvOs"
                Accessible.name: qsTr("BvOs Intelligence Read Aloud")
                textFormat: Text.PlainText
                color: card.ink
                font.family: "Inter"
                font.weight: Font.Medium
                font.pixelSize: 13
            }
            Text {
                text: qsTr("v%1").arg(Qt.application.version)
                textFormat: Text.PlainText
                color: card.muted
                font.pixelSize: 9
            }
            Button {
                id: waveformButton
                Layout.preferredWidth: 23
                Layout.preferredHeight: 23
                enabled: reader.playable
                padding: 0
                focusPolicy: Qt.NoFocus
                Accessible.name: reader.wantsPlayback ? qsTr("Pause") : qsTr("Play")
                CardTip { visible: waveformButton.hovered; text: waveformButton.Accessible.name }
                onClicked: reader.toggle()
                background: Rectangle {
                    radius: width / 2
                    color: waveformButton.hovered ? Qt.alpha(card.accent, 0.15) : "transparent"
                }

                contentItem: Item {
                    Row {
                        anchors.centerIn: parent
                        height: 14
                        spacing: 2
                        visible: reader.speaking

                        Repeater {
                            model: [6, 10, 14, 10, 6]

                            Rectangle {
                                required property int index
                                required property int modelData
                                anchors.verticalCenter: parent.verticalCenter
                                width: 2
                                height: 3
                                radius: width / 2
                                color: "#78d958"

                                SequentialAnimation on height {
                                    running: reader.speaking
                                    loops: Animation.Infinite
                                    NumberAnimation {
                                        to: modelData
                                        duration: 165 + index * 28
                                        easing.type: Easing.InOutSine
                                    }
                                    NumberAnimation {
                                        to: 3
                                        duration: 165 + (4 - index) * 28
                                        easing.type: Easing.InOutSine
                                    }
                                }
                            }
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: waveformButton.enabled && !reader.speaking
                        text: reader.wantsPlayback ? "Ⅱ" : "▸"
                        color: reader.paused ? (reader.dark ? "#f07b7b" : "#dc4a4a") : card.accent
                        font.pixelSize: 15
                    }
                }
            }
            Item { Layout.fillWidth: true }

            Text {
                text: reader.total > 0 ? qsTr("%1 / %2").arg(reader.sentence).arg(reader.total) : ""
                textFormat: Text.PlainText
                color: card.muted
                font.pixelSize: 11
            }
            PlayerButton {
                visible: card.expanded
                symbol: card.fullscreen ? "restore" : "fullscreen"
                label: card.fullscreen ? qsTr("Exit fullscreen (Esc)") : qsTr("Fullscreen")
                onClicked: reader.setCardFullscreen(!card.fullscreen)
            }

            Button {
                id: closeButton
                visible: card.expanded
                Layout.preferredWidth: 23
                Layout.preferredHeight: 23
                Layout.rightMargin: 5
                padding: 0
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Close Read Aloud")
                CardTip { visible: closeButton.hovered; text: closeButton.Accessible.name }
                contentItem: Text {
                    text: "×"
                    color: closeButton.hovered ? card.ink : card.muted
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: width / 2
                    color: closeButton.hovered ? Qt.alpha(card.accent, 0.15) : "transparent"
                }
                onClicked: card.close()
            }
            Item {
                visible: !card.expanded
                Layout.preferredWidth: 4
            }

            DragHandler {
                id: headerDrag
                target: null
                enabled: !card.fullscreen
                acceptedButtons: Qt.LeftButton
                cursorShape: active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                onActiveChanged: {
                    if (active)
                        card.startSystemMove()
                    else if (!hover.hovered)
                        collapse.restart()
                }
            }
        }

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 34
            anchors.leftMargin: card.fullscreen ? card.passageSideMargin : 9
            anchors.rightMargin: card.fullscreen ? card.passageSideMargin : 7
            height: 28
            spacing: 2
            visible: card.expanded

            Text {
                id: statusLabel
                Layout.fillWidth: true
                text: reader.status
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: card.muted
                font.pixelSize: 11
                CardTip { visible: statusHover.hovered && statusLabel.truncated; text: reader.status }
                HoverHandler { id: statusHover }
            }

            PlayerButton {
                symbol: "previous"
                label: qsTr("Previous sentence (Ctrl+Left)")
                enabled: reader.playable
                onClicked: reader.previous()
            }
            PlayerButton {
                primary: true
                symbol: reader.wantsPlayback ? "pause" : "play"
                label: reader.wantsPlayback ? qsTr("Pause (Ctrl+Down)") : qsTr("Play (Ctrl+Up)")
                enabled: reader.playable
                onClicked: reader.toggle()
            }
            PlayerButton {
                symbol: "next"
                label: qsTr("Next sentence (Ctrl+Right)")
                enabled: reader.sentence < reader.total
                onClicked: reader.next()
            }
            Button {
                id: opacityButton
                visible: card.expanded && reader.opacityAvailable
                Layout.preferredWidth: 23
                Layout.preferredHeight: 23
                padding: 0
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Window opacity")
                CardTip { visible: opacityButton.hovered; text: opacityButton.Accessible.name }
                contentItem: Text {
                    text: "◐"
                    color: opacityButton.hovered ? card.ink : card.muted
                    font.pixelSize: 15
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: width / 2
                    color: opacityButton.hovered ? Qt.alpha(card.accent, 0.15) : "transparent"
                }
                onClicked: reader.openOpacity()
            }

            PlayerButton {
                symbol: "stop"
                label: qsTr("Stop and close")
                onClicked: card.close()
            }
        }
        Button {
            id: previewMode
            anchors.top: parent.top
            anchors.topMargin: 64
            anchors.right: parent.right
            anchors.rightMargin: card.passageSideMargin
            height: 21
            padding: 4
            visible: card.expanded && reader.hasPreview
            text: card.editingPreview ? qsTr("Done") : qsTr("Edit")
            focusPolicy: Qt.NoFocus
            contentItem: Text {
                text: previewMode.text
                color: card.accent
                font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
            }
            background: null
            onClicked: {
                card.editingPreview = !card.editingPreview
                previewFlick.contentY = 0
                if (card.editingPreview)
                    previewEditor.forceActiveFocus()
                else
                    previewEditor.focus = false
            }
        }

        ScrollView {
            id: passagePreview
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: card.passageSideMargin
            anchors.rightMargin: card.passageSideMargin
            anchors.bottomMargin: card.fullscreen ? 24 : 10
            anchors.topMargin: 87
            visible: card.expanded && reader.hasPreview
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            Accessible.name: qsTr("Text being read")
            onVisibleChanged: {
                if (visible)
                    Qt.callLater(followHighlight)
                else
                    previewEditor.focus = false
            }

            function followHighlight() {
                if (!visible || card.editingPreview || reader.highlightPosition < 0)
                    return
                const line = renderedPreview.positionToRectangle(reader.highlightPosition)
                if (line.y < previewFlick.contentY
                    || line.y + line.height > previewFlick.contentY + previewFlick.height) {
                    const bottom = Math.max(0, previewFlick.contentHeight - previewFlick.height)
                    previewFlick.contentY = Math.max(0, Math.min(line.y - 4, bottom))
                }
            }

            Flickable {
                id: previewFlick
                contentWidth: width
                contentHeight: card.editingPreview ? previewEditor.height : renderedPreview.height
                boundsBehavior: Flickable.StopAtBounds

                TextEdit {
                    id: renderedPreview
                    readOnly: true
                    activeFocusOnPress: false
                    height: contentHeight
                    visible: !card.editingPreview
                    width: previewFlick.width - 8
                    text: card.editingPreview ? "" : reader.formattedPreview
                    textFormat: TextEdit.RichText
                    wrapMode: TextEdit.Wrap
                    color: card.ink
                    font.pixelSize: card.fullscreen ? 19 : 12
                }

                TextArea {
                    id: previewEditor
                    visible: card.editingPreview
                    width: previewFlick.width - 8
                    height: Math.max(implicitHeight, previewFlick.height)
                    text: reader.preview
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    color: card.ink
                    selectionColor: card.accent
                    selectedTextColor: reader.dark ? "#0f1724" : "#ffffff"
                    padding: 3
                    font.pixelSize: card.fullscreen ? 19 : 12
                    background: null
                    onTextChanged: if (activeFocus && text !== reader.preview) reader.editPreview(text)
                    onCursorRectangleChanged: {
                        if (!activeFocus)
                            return
                        if (cursorRectangle.y < previewFlick.contentY)
                            previewFlick.contentY = cursorRectangle.y
                        else if (cursorRectangle.y + cursorRectangle.height > previewFlick.contentY + previewFlick.height)
                            previewFlick.contentY = cursorRectangle.y + cursorRectangle.height - previewFlick.height
                    }
                }
            }

            Connections {
                target: reader
                function onFormattedPreviewChanged() { Qt.callLater(passagePreview.followHighlight) }
                function onPreviewReplaced() {
                    card.editingPreview = false
                    previewEditor.cursorPosition = 0
                    previewFlick.contentY = 0
                }
            }
        }
    }
}
