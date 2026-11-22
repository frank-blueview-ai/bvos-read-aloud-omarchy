import QtQuick
import Quickshell
import qs.Commons as Commons
import qs.Ui

BarWidget {
    id: root
    moduleName: "blueview.read-aloud"
    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    Commons.Translations {
        id: strings
        directory: Qt.resolvedUrl("i18n")
    }

    BarIconButton {
        id: button
        anchors.fill: parent
        bar: root.bar
        iconComponent: Component {
            Image {
                width: 24
                height: 24
                fillMode: Image.PreserveAspectFit
                source: Qt.resolvedUrl("assets/double-oval.svg")
            }
        }
        Accessible.name: strings.t("visitBlueView")
        onPressed: function(mouseButton) {
            if (mouseButton === Qt.LeftButton) {
                Qt.openUrlExternally("https://blueview.ai")
                return
            }
            const executable = Quickshell.env("HOME") + "/.local/bin/omarchy-read-aloud"
            Quickshell.execDetached(mouseButton === Qt.RightButton
                ? [executable, "--clipboard"] : [executable])
        }
    }
}
