import QtQuick 2.0
import Sailfish.Silica 1.0
import ru.tk.FruktAImeter 1.0
import QtMultimedia 5.6
import Qt.labs.folderlistmodel 2.1

Page {
    objectName: "mainPage"
    allowedOrientations: Orientation.All

    // Детектор яблок
    Detector {
        id: detector
        
        onAnalysisComplete: {
            resultLabel.text = "Качество: " + quality
            confidenceLabel.text = "Уверенность: " + (confidence * 100).toFixed(1) + "%"
            busyIndicator.running = false
        }
        
        onAnalysisError: {
            resultLabel.text = "Ошибка: " + errorMessage
            confidenceLabel.text = ""
            busyIndicator.running = false
        }
    }

    // Диалог выбора файла
    Component {
        id: filePickerDialog
        
        Dialog {
            allowedOrientations: Orientation.All
            
            SilicaListView {
                anchors.fill: parent
                
                header: DialogHeader {
                    title: "Выберите изображение"
                }
                
                model: FolderListModel {
                    id: folderModel
                    folder: "file:///home/defaultuser/Pictures"
                    nameFilters: ["*.jpg", "*.jpeg", "*.png", "*.JPG", "*.JPEG", "*.PNG"]
                    showDirs: false
                }
                
                delegate: ListItem {
                    contentHeight: Theme.itemSizeSmall
                    
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.horizontalPageMargin
                        text: model.fileName
                        color: highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                    
                    onClicked: {
                        busyIndicator.running = true
                        resultLabel.text = ""
                        confidenceLabel.text = ""
                        detector.analyzeFromPath(model.fileURL)
                        pageStack.pop()
                    }
                }
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                objectName: "pageHeader"
                title: qsTr("FruktAImeter")
                description: qsTr("Анализатор качества яблок")
                extraContent.children: [
                    IconButton {
                        objectName: "aboutButton"
                        icon.source: "image://theme/icon-m-about"
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
                    }
                ]
            }

            // Превью изображения
            Image {
                id: previewImage
                width: parent.width
                height: width * 0.75
                fillMode: Image.PreserveAspectFit
                source: ""
                visible: source != ""
                
                BusyIndicator {
                    id: busyIndicator
                    anchors.centerIn: parent
                    size: BusyIndicatorSize.Large
                    running: false
                }
            }

            // Кнопка камеры
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "📷 Сфотографировать яблоко"
                preferredWidth: Theme.buttonWidthLarge
                
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("CameraPage.qml"), {
                        detector: detector
                    })
                }
            }

            // Кнопка галереи
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "🖼️ Выбрать из галереи"
                preferredWidth: Theme.buttonWidthLarge
                
                onClicked: {
                    pageStack.push(filePickerDialog)
                }
            }

            // Тестовые изображения
            SectionHeader {
                text: "Тестирование"
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingMedium

                Button {
                    text: "🍏 Хорошее"
                    onClicked: {
                        busyIndicator.running = true
                        resultLabel.text = ""
                        confidenceLabel.text = ""
                        detector.analyzeTestImage(0)
                    }
                }

                Button {
                    text: "🍎 Плохое"
                    onClicked: {
                        busyIndicator.running = true
                        resultLabel.text = ""
                        confidenceLabel.text = ""
                        detector.analyzeTestImage(1)
                    }
                }
            }

            // Результаты анализа
            Column {
                width: parent.width
                spacing: Theme.paddingMedium
                visible: resultLabel.text !== ""

                SectionHeader {
                    text: "Результат анализа"
                }

                Label {
                    id: resultLabel
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeLarge
                    font.bold: true
                    color: Theme.highlightColor
                    text: ""
                }

                Label {
                    id: confidenceLabel
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeMedium
                    color: Theme.secondaryHighlightColor
                    text: ""
                }

                // Кнопки коррекции
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Theme.paddingLarge
                    visible: resultLabel.text !== ""

                    Button {
                        text: "✓ Верно"
                        color: Theme.rgba(Theme.highlightBackgroundColor, 0.2)
                        onClicked: {
                            detector.fixMistake(true)
                            resultLabel.text = "Спасибо за обратную связь!"
                            confidenceLabel.text = ""
                        }
                    }

                    Button {
                        text: "✗ Ошибка"
                        color: Theme.rgba(Theme.errorColor, 0.2)
                        onClicked: {
                            detector.fixMistake(false)
                            resultLabel.text = "Модель обучена, спасибо!"
                            confidenceLabel.text = ""
                        }
                    }
                }
            }

            // Инструкция
            Label {
                anchors {
                    left: parent.left
                    right: parent.right
                    margins: Theme.horizontalPageMargin
                }
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                text: qsTr("Сфотографируйте яблоко или выберите изображение из галереи. " +
                          "Приложение определит качество яблока с помощью нейросети.")
            }
        }
    }
}
