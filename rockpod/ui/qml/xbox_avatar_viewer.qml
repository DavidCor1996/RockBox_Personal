import QtQuick
import QtQuick3D

Rectangle {
    id: root
    color: "#d9dddf"

    property url modelSource
    property color skinTint: "white"
    property color hairTint: "white"
    property color topTint: "white"
    property color bottomTint: "white"
    property color shoesTint: "white"
    property real yaw: 0
    property bool autoRotate: true

    function resetRotation() {
        yaw = 0
        autoRotate = true
    }

    View3D {
        anchors.fill: parent
        environment: SceneEnvironment {
            clearColor: "#d9dddf"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.VeryHigh
            aoStrength: 48
            aoDistance: 1.8
            aoSoftness: 72
        }

        PerspectiveCamera {
            position: Qt.vector3d(0, 78, 215)
            clipNear: 1
            clipFar: 1000
            fieldOfView: 42
        }

        DirectionalLight {
            eulerRotation.x: -28
            eulerRotation.y: -32
            brightness: 1.25
            castsShadow: true
            shadowFactor: 62
            shadowMapQuality: Light.ShadowMapQualityHigh
        }
        DirectionalLight {
            eulerRotation.x: 18
            eulerRotation.y: 145
            brightness: 0.42
        }

        Node {
            eulerRotation.y: root.yaw
            Loader3D {
                id: avatarLoader
                source: root.modelSource
            }
            Binding {
                target: avatarLoader.item
                property: "skinTint"
                value: root.skinTint
                when: avatarLoader.status === Loader3D.Ready
            }
            Binding {
                target: avatarLoader.item
                property: "hairTint"
                value: root.hairTint
                when: avatarLoader.status === Loader3D.Ready
            }
            Binding {
                target: avatarLoader.item
                property: "topTint"
                value: root.topTint
                when: avatarLoader.status === Loader3D.Ready
            }
            Binding {
                target: avatarLoader.item
                property: "bottomTint"
                value: root.bottomTint
                when: avatarLoader.status === Loader3D.Ready
            }
            Binding {
                target: avatarLoader.item
                property: "shoesTint"
                value: root.shoesTint
                when: avatarLoader.status === Loader3D.Ready
            }
        }

        Model {
            source: "#Rectangle"
            y: -0.3
            eulerRotation.x: -90
            scale: Qt.vector3d(1.8, 1.8, 1.8)
            receivesShadows: true
            materials: PrincipledMaterial {
                baseColor: "#78a92b"
                roughness: 0.82
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 34
        color: "#b41d2226"
        Text {
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 12
            font.bold: true
            text: "DRAG TO ROTATE 360°  ·  DOUBLE-CLICK TO RESET"
        }
    }

    NumberAnimation on yaw {
        from: 0
        to: 360
        duration: 12000
        loops: Animation.Infinite
        running: root.autoRotate
    }

    MouseArea {
        anchors.fill: parent
        property real lastX
        onPressed: function(mouse) {
            root.autoRotate = false
            lastX = mouse.x
        }
        onPositionChanged: function(mouse) {
            if (pressed) {
                root.yaw = (root.yaw + (mouse.x - lastX) * 0.75) % 360
                lastX = mouse.x
            }
        }
        onDoubleClicked: root.resetRotation()
    }
}
