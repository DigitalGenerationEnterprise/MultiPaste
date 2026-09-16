import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents

Item {
    id: root
    Layout.minimumWidth: 220
    Layout.minimumHeight: 260
    Layout.preferredWidth: 300
    Layout.preferredHeight: 320

    // Plasma 5 build: PlasmaCore.DataSource ("executable" engine) and
    // Plasmoid.file() are both available on KF5.
    readonly property string statusScript: Plasmoid.file("code", "status.sh")
    readonly property string pasteScript: Plasmoid.file("code", "paste.sh")
    readonly property var helpers: [
        pasteScript, // helper[0] -> paste
        Plasmoid.file("code", "newseq.sh"), // helper[1]
        Plasmoid.file("code", "clear.sh")   // helper[2]
    ]

    property var snap: ({"items": [], "next": 0, "enabled": false, "activity": []})

    PlasmaCore.DataSource {
        id: runner
        engine: "executable"
        connectedSources: [root.statusScript]
        onNewData: {
            var text = (data && data.stdout) ? data.stdout : "";
            if (!text) return;
            try {
                root.snap = JSON.parse(text);
            } catch (e) {
                console.warn("MultiPaste widget: unparsable status", text);
            }
        }
    }

    function command(script) {
        runner.connectSource(script);
    }

    Timer {
        interval: 3000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: root.command(root.statusScript)
    }

    function action(i) {
        root.command(root.helpers[i]);
    }

    function label(i) {
        var it = root.snap.items[i];
        if (!it) return "";
        var t = it.description !== undefined ? it.description : it;
        if (t.length > 34) t = t.substring(0, 31) + "…";
        return (i + 1) + "  " + t;
    }

    function nextLabel() {
        if (!root.snap.items || root.snap.items.length === 0) return "-";
        var it = root.snap.items[root.snap.next];
        return it !== undefined ? root.label(root.snap.next) : "-";
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            PlasmaComponents.Label {
                text: "MultiPaste"
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            PlasmaComponents.Label {
                text: root.snap.enabled
                    ? "next: " + root.nextLabel()
                    : "disabled"
                color: root.snap.enabled ? PlasmaCore.Theme.highlightColor
                                         : PlasmaCore.Theme.negativeTextColor
                font.bold: true
            }
            PlasmaComponents.Label {
                text: root.snap.status === "off" ? "· not running" : ""
                color: PlasmaCore.Theme.negativeTextColor
            }
        }

        Repeater {
            model: Math.min(root.snap.items.length, 6)
            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                radius: 3
                color: root.snap.items[index] && index === root.snap.next
                    ? PlasmaCore.Theme.highlightColor
                    : PlasmaCore.Theme.backgroundColor
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 4
                    PlasmaComponents.Label {
                        text: root.label(index)
                        color: (root.snap.items[index] && index === root.snap.next)
                            ? PlasmaCore.Theme.highlightedTextColor
                            : PlasmaCore.Theme.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }
        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: root.snap.items.length === 0 && root.snap.status !== "off"
            text: "Nothing copied yet"
            color: PlasmaCore.Theme.disabledTextColor
        }
        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: root.snap.items.length > 6
            text: "…and " + (root.snap.items.length - 6) + " more in the tray"
            color: PlasmaCore.Theme.disabledTextColor
            font.pointSize: 8
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: PlasmaCore.Theme.backgroundColor
            radius: 3
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 1
                Repeater {
                    model: Math.min(root.snap.activity.length, 8)
                    delegate: PlasmaComponents.Label {
                        text: root.snap.activity[index]
                        font.pointSize: 8
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        color: PlasmaCore.Theme.textColor
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            PlasmaComponents.Button {
                text: "Paste Next"
                enabled: root.snap.items.length > 0 && root.snap.enabled
                onClicked: root.action(0)
            }
            PlasmaComponents.Button {
                text: "New Seq"
                onClicked: root.action(1)
            }
            PlasmaComponents.Button {
                text: "Clear"
                onClicked: root.action(2)
            }
        }
    }

    Component.onCompleted: root.command(root.statusScript)
}