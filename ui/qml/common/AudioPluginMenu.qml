import "." as Local
import QtQuick
import QtQuick.Controls

Menu {
    id: root

    signal pluginSelected(string pluginId)

    title: "オーディオプラグイン"

    Menu {
        title: "カテゴリから選択"
        icon.name: "folder_line"

        Repeater {
            model: TimelineBridge ? TimelineBridge.getPluginCategories() : []

            Menu {
                title: modelData

                Repeater {
                    model: TimelineBridge ? TimelineBridge.getPluginsByCategory(title) : []

                    Local.IconMenuItem {
                        text: modelData.name
                        iconName: "music_line"
                        onTriggered: {
                            root.pluginSelected(modelData.id);
                            root.dismiss();
                        }
                    }

                }

            }

        }

    }

    Menu {
        title: "フォーマットから選択"
        icon.name: "plug_line"

        Repeater {
            model: TimelineBridge ? TimelineBridge.getPluginFormats() : []

            Menu {
                title: modelData

                Repeater {
                    model: TimelineBridge ? TimelineBridge.getPluginsByFormat(title) : []

                    Local.IconMenuItem {
                        text: modelData.name
                        iconName: "music_line"
                        onTriggered: {
                            root.pluginSelected(modelData.id);
                            root.dismiss();
                        }
                    }

                }

            }

        }

    }

}
