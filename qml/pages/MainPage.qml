import QtQuick 2.0
import Sailfish.Silica 1.0
import ru.tk.FruktAImeter 1.0
import QtMultimedia 5.6
import Qt.labs.folderlistmodel 2.1

Page {
    objectName: "mainPage"
    allowedOrientations: Orientation.All

    // Текущий путь к анализируемому изображению
    property string currentImagePath: ""

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
    
    // Функция для анализа тестового изображения с превью
    function analyzeTestWithPreview(imageType) {
        busyIndicator.running = true
        resultLabel.text = ""
        confidenceLabel.text = ""
        
        // Показываем превью из ресурсов
        if (imageType === 0) {
            previewImage.source = "qrc:/images/test_apple_good.jpg"
        } else {
            previewImage.source = "qrc:/images/test_apple_bad.jpg"
        }
        
        detector.analyzeTestImage(imageType)
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
                        previewImage.source = model.fileURL
                        detector.analyzeFromPath(model.filePath)
                        pageStack.pop()
                    }
                }
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        PullDownMenu {
            MenuItem {
                text: qsTr("О приложении")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
        }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                objectName: "pageHeader"
                title: qsTr("FruktAImeter")
                description: qsTr("Анализатор качества яблок")
            }

            // Превью изображения
            Rectangle {
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: width * 0.75
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.rgba(Theme.highlightBackgroundColor, 0.1)
                radius: Theme.paddingSmall
                
                Image {
                    id: previewImage
                    anchors.fill: parent
                    anchors.margins: Theme.paddingSmall
                    fillMode: Image.PreserveAspectFit
                    source: ""
                    
                    // Placeholder когда нет изображения
                    Label {
                        anchors.centerIn: parent
                        text: "🍎"
                        font.pixelSize: Theme.fontSizeHuge * 3
                        visible: previewImage.source == ""
                        opacity: 0.3
                    }
                }
                
                BusyIndicator {
                    id: busyIndicator
                    anchors.centerIn: parent
                    size: BusyIndicatorSize.Large
                    running: false
                }
            }

            // Результаты анализа (перенесены выше для лучшей видимости)
            Column {
                width: parent.width
                spacing: Theme.paddingSmall
                visible: resultLabel.text !== ""

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
            }

            // Кнопки коррекции
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingLarge
                visible: resultLabel.text !== "" && resultLabel.text.indexOf("Качество:") === 0

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

            // Разделитель
            Item { width: 1; height: Theme.paddingMedium }

            // Кнопка камеры
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "📷 Сфотографировать"
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
                text: "🖼️ Из галереи"
                preferredWidth: Theme.buttonWidthLarge
                
                onClicked: {
                    pageStack.push(filePickerDialog)
                }
            }

            // Тестовые изображения
            SectionHeader {
                text: "Тестовые изображения"
            }
            
            Label {
                anchors {
                    left: parent.left
                    right: parent.right
                    margins: Theme.horizontalPageMargin
                }
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: "Нажмите для загрузки тестового изображения и анализа:"
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingMedium

                Button {
                    text: "🍏 Хорошее яблоко"
                    onClicked: analyzeTestWithPreview(0)
                }

                Button {
                    text: "🍎 Плохое яблоко"
                    onClicked: analyzeTestWithPreview(1)
                }
            }

            // Инструкция
            Item { width: 1; height: Theme.paddingLarge }
            
            Label {
                anchors {
                    left: parent.left
                    right: parent.right
                    margins: Theme.horizontalPageMargin
                }
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                text: qsTr("Сфотографируйте яблоко или выберите изображение. " +
                          "Нейросеть YOLO найдёт яблоко на фото, а KNN-классификатор " +
                          "определит его качество. Если результат неверный — нажмите " +
                          "'Ошибка' для дообучения модели.")
            }
            
            // Отступ снизу
            Item { width: 1; height: Theme.paddingLarge }
        }
    }
}
