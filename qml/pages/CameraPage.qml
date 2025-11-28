import QtQuick 2.0
import Sailfish.Silica 1.0
import QtMultimedia 5.6

Page {
    id: cameraPage
    allowedOrientations: Orientation.All
    
    property var detector
    
    Camera {
        id: camera
        
        captureMode: Camera.CaptureStillImage
        
        imageCapture {
            onImageSaved: {
                console.log("Image saved:", path)
                busyIndicator.running = true
                detector.analyzeFromPath(path)
                pageStack.pop()
            }
        }
    }
    
    VideoOutput {
        id: videoOutput
        source: camera
        anchors.fill: parent
        focus: visible
        orientation: -90
    }
    
    Column {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: Theme.paddingLarge
        spacing: Theme.paddingLarge
        
        // Кнопка съёмки
        IconButton {
            anchors.horizontalCenter: parent.horizontalCenter
            icon.source: "image://theme/icon-camera-shutter"
            icon.width: Theme.iconSizeExtraLarge
            icon.height: Theme.iconSizeExtraLarge
            
            onClicked: {
                camera.imageCapture.capture()
            }
        }
        
        // Кнопка отмены
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Отмена"
            onClicked: pageStack.pop()
        }
    }
    
    BusyIndicator {
        id: busyIndicator
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: false
    }
    
    Component.onCompleted: {
        camera.start()
    }
    
    Component.onDestruction: {
        camera.stop()
    }
}
