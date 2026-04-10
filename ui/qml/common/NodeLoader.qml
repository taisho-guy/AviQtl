import "Logger.js" as Logger
import QtQuick
import QtQuick3D

Node {
    id: loader

    property url source
    property var properties: ({
    })
    property QtObject item: null
    property int status: Loader.Null
    property string errorString: ""
    // 外部からコンポーネント取得関数を注入可能にする
    // function(url) -> Component
    property var componentFactory: null
    property Component _component: null

    function _applyProperties() {
        if (!item || !properties)
            return ;

        for (var key in properties) {
            if (item.hasOwnProperty(key) || (key in item))
                item[key] = properties[key];

        }
    }

    function _load() {
        if (item) {
            item.destroy();
            item = null;
        }
        _component = null;
        status = Loader.Loading;
        errorString = "";
        if (source == "") {
            status = Loader.Null;
            return ;
        }
        var comp = null;
        if (componentFactory && typeof componentFactory === "function")
            comp = componentFactory(source);
        else
            comp = Qt.createComponent(source, Component.Asynchronous);
        _component = comp;
        _processStatus();
    }

    function _processStatus() {
        if (!_component)
            return ;

        status = _component.status;
        if (status === Component.Ready) {
            item = _component.createObject(loader, properties);
            _applyProperties();
        } else if (status === Component.Error) {
            errorString = _component.errorString();
            Logger.log(qsTr("[NodeLoader] コンポーネントエラー: %1").arg(errorString));
        }
    }

    onSourceChanged: _load()
    onPropertiesChanged: _applyProperties()

    Connections {
        function onStatusChanged() {
            loader._processStatus();
        }

        target: _component
    }

}
