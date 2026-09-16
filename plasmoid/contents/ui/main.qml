import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.plasma.plasma5support 2.0 as PlasmaSupport

Item {
    id: root
    Layout.minimumWidth: 220
    Layout.minimumHeight: 260
    Layout.preferredWidth: 300
    Layout.preferredHeight: 320

    // Plasma 6 build: DataSource/"executable" live in org.kde.plasma.plasma5support.
    // Plasmoid.file() was removed in Plasma 6, so bridge scripts are resolved
    // relative to this file's location (contents/code/) instead.
    function codeFile(name) {
        return Qt.resolvedUrl("../code/" + name).toString().replace(/^file:\/\//, "");
    }

    readonly property string statusScript: codeFile("status.sh")
    readonly property string pasteScript: codeFile("paste.sh")
    readonly property var helpers: [
        pasteScript, // helper[0] -> paste
        codeFile("newseq.sh"), // helper[1]
        codeFile("clear.sh")   // helper[2]
    ]

    property var snap: ({"items": [], "next": 0, "enabled": false, "activity": []})

    // Theme-independent palette (SystemPalette is available on Qt 5.15+/6.x
    // whereas PlasmaCore.Theme was removed from org.kde.plasma.core in Plasma 6).
    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }
    readonly property color highlightColor: systemPalette.highlight
    readonly property color highlightedTextColor: systemPalette.highlightedText
    readonly property color textColor: systemPalette.windowText
    readonly property color backgroundColor: systemPalette.window
    readonly property color disabledTextColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.55)
    readonly property color negativeTextColor: Qt.darker(systemPalette.windowText, 1.6)

    PlasmaSupport.DataSource {
        id: runner
        engine: "executable"
        onNewData: { /* consumed below */ }
    }

    function command(script) {
        runner.connectSource(script);
    }

    function refresh() {
        command(statusScript);
    }

    // Executable engine: the latest reaction to status.sh is parsed here.
    Connections {
        target: runner
        function onNewData(source, data) {
            var text = (data && data.stdout) ? data.stdout : "";
            if (!text) return;
            try {
                root.snap = JSON.parse(text);
            } catch (e) {
                console.warn("MultiPaste widget: unparsable status", text);
            }
        }
    }

    Timer {
        interval: 3000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: root.refresh()
    }

    function action(i) {
        command(root.helpers[i]);
        // Reflect the action locally right away; the periodic refresh syncs
        // with the app's own state shortly after.
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

        // Title + next pointer.
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
                color: root.snap.enabled ? root.highlightColor
                                         : root.negativeTextColor
                font.bold: true
            }
            PlasmaComponents.Label {
                text: root.snap.status === "off" ? "· not running" : ""
                color: root.negativeTextColor
            }
        }

        // Current copy order (FIFO preview).
        Repeater {
            model: Math.min(root.snap.items.length, 6)
            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                radius: 3
                color: root.snap.items[index] && index === root.snap.next
                    ? root.highlightColor
                    : root.backgroundColor
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 4
                    PlasmaComponents.Label {
                        text: root.label(index)
                        color: (root.snap.items[index] && index === root.snap.next)
                            ? root.highlightedTextColor
                            : root.textColor
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
            color: root.disabledTextColor
        }
        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: root.snap.items.length > 6
            text: "…and " + (root.snap.items.length - 6) + " more in the tray"
            color: root.disabledTextColor
            font.pointSize: 8
        }

        // Recent activity log.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: root.backgroundColor
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
                        color: root.textColor
                    }
                }
            }
        }

        // Buttons.
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

}

