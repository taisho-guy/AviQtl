import QtQuick
import QtQuick.Window

Window {
    id: splashWindow

    property string currentLog: "Initializing Rina..."
    property string nextLog: ""

    function closeSplash() {
        closeAnim.start();
    }

    width: 400
    height: 300
    color: "transparent"
    flags: Qt.SplashScreen | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visible: true

    // C++ から呼ばれる関数群
    Connections {
        function onLogMessage(msg) {
            if (splashWindow.currentLog === msg)
                return ;

            splashWindow.nextLog = msg;
            transitionAnim.restart();
        }

        target: SplashLogger
    }

    Rectangle {
        id: bgRect

        anchors.fill: parent
        color: "#1e1e1e" // palette.window が取れない場合のフォールバックを兼ねる
        radius: 12
        border.color: "#333333"
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: 20

            Image {
                source: "qrc:/assets/icon.svg"
                width: 128
                height: 128
                anchors.horizontalCenter: parent.horizontalCenter
                sourceSize.width: 256
                sourceSize.height: 256
                smooth: true
                antialiasing: true
            }

            // ログの電光掲示板エリア
            Item {
                width: 300
                height: 30
                anchors.horizontalCenter: parent.horizontalCenter
                clip: true
                // 両端を滑らかにフェードさせるマスク
                layer.enabled: true

                // 現在のテキスト
                Text {
                    id: currentText

                    text: splashWindow.currentLog
                    color: "#8000ff"
                    font.pixelSize: 14
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                    x: (parent.width - width) / 2
                }

                // 次のテキスト
                Text {
                    id: nextText

                    text: splashWindow.nextLog
                    color: "#8000ff"
                    font.pixelSize: 14
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                    x: parent.width + 50
                    visible: false
                }

                layer.effect: ShaderEffect {
                    fragmentShader: "
                        varying highp vec2 qt_TexCoord0;
                        uniform sampler2D source;
                        void main() {
                            lowp vec4 texColor = texture2D(source, qt_TexCoord0);
                            highp float edge = 0.15; // 15% の幅でフェード
                            highp float alpha = smoothstep(0.0, edge, qt_TexCoord0.x) * smoothstep(1.0, 1.0 - edge, qt_TexCoord0.x);
                            gl_FragColor = texColor * alpha;
                        }
                    "
                }

            }

        }

    }

    // ログ切り替え時の「一気にシュッとなって次のログが来る」アニメーション
    SequentialAnimation {
        id: transitionAnim

        // 1. 現在のテキストが左に加速して消える
        ParallelAnimation {
            NumberAnimation {
                target: currentText
                property: "x"
                to: -currentText.width - 150
                duration: 250
                easing.type: Easing.InBack
            }

            NumberAnimation {
                target: currentText
                property: "opacity"
                to: 0
                duration: 200
            }

        }

        // 2. テキストの入れ替えと位置リセット
        ScriptAction {
            script: {
                currentText.text = splashWindow.nextLog;
                currentText.x = 350; // 右端の外にセット
                currentText.opacity = 1;
            }
        }

        // 3. 新しいテキストが右から中央へ減速して入ってくる
        NumberAnimation {
            target: currentText
            property: "x"
            to: (300 - currentText.contentWidth) / 2
            duration: 350
            easing.type: Easing.OutBack
            easing.overshoot: 1.2
        }

    }

    // 起動完了時のフェードアウト
    NumberAnimation {
        id: closeAnim

        target: splashWindow
        property: "opacity"
        to: 0
        duration: 300
        onFinished: splashWindow.destroy()
    }

}
