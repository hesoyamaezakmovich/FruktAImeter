#include "Brain.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QFile>

Brain::Brain() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);
    m_path = dataDir + "/brain_memory.xml"; 
    load();
}

void Brain::load() {
    bool loadedFromUser = false;

    // 1. Проверяем пользовательскую память (дообученную)
    if (QFile::exists(m_path)) {
        try {
            mlpack::data::Load(m_path.toStdString(), "dataset", m_dataset, true);
            mlpack::data::Load(m_path.toStdString(), "labels", m_labels, true);
            if (m_dataset.n_cols > 0) loadedFromUser = true;
        } catch (...) { qDebug() << "Error reading user memory"; }
    }

    // 2. Если пусто — грузим заводские CSV из ресурсов
    if (!loadedFromUser) {
        qDebug() << "Brain: Loading factory CSV...";
        
        // Копируем во временные файлы (MLPack не читает из qrc)
        QString tmpData = QDir::tempPath() + "/dataset.csv";
        QString tmpLabels = QDir::tempPath() + "/labels.csv";

        if (QFile::exists(":/data/dataset.csv")) {
            QFile::remove(tmpData); QFile::remove(tmpLabels);
            QFile::copy(":/data/dataset.csv", tmpData);
            QFile::copy(":/data/labels.csv", tmpLabels);
            QFile::setPermissions(tmpData, QFile::ReadOwner | QFile::WriteOwner);
            QFile::setPermissions(tmpLabels, QFile::ReadOwner | QFile::WriteOwner);
            
            try {
                // Загружаем (MLpack автоматически транспонирует CSV)
                // CSV формат: строки = примеры, столбцы = признаки
                // MLpack формат: колонки = примеры, строки = признаки
                mlpack::data::Load(tmpData.toStdString(), m_dataset, true);
                
                // labels.csv: одна колонка с метками
                arma::mat labelsTemp;
                mlpack::data::Load(tmpLabels.toStdString(), labelsTemp, true);
                
                // Преобразуем в Row<size_t>
                m_labels = arma::conv_to<arma::Row<size_t>>::from(labelsTemp.row(0));

                qDebug() << "Brain: Factory data loaded! Samples:" << m_dataset.n_cols 
                         << "Features:" << m_dataset.n_rows;
            } catch (const std::exception& e) {
                qDebug() << "Brain: ERROR loading CSV:" << e.what();
            }
        } else {
            qDebug() << "Brain: WARNING! dataset.csv not found in assets!";
        }
    }
}

void Brain::save() {
    mlpack::data::Save(m_path.toStdString(), "dataset", m_dataset);
    mlpack::data::Save(m_path.toStdString(), "labels", m_labels);
}

int Brain::classify(const arma::rowvec& features) {
    if (m_dataset.n_cols < 1) return 1; // По умолчанию ок

    // MLpack 4.x API: KNN без namespace neighbor
    mlpack::KNN knn(m_dataset);
    arma::Mat<size_t> neighbors;
    arma::mat distances;

    size_t k = (m_dataset.n_cols < 5) ? m_dataset.n_cols : 5;
    knn.Search(features.t(), k, neighbors, distances);

    int score = 0;
    for (size_t i = 0; i < neighbors.n_elem; ++i) {
        if (m_labels[neighbors(i)] == 1) score++;
        else score--;
    }
    // Если поровну, верим самому близкому
    if (score == 0) return (m_labels[neighbors(0)] == 1) ? 1 : 0;
    
    return (score > 0) ? 1 : 0;
}

void Brain::learn(const arma::rowvec& features, int label) {
    m_dataset.insert_cols(m_dataset.n_cols, features.t());
    arma::Row<size_t> newLabel = { (size_t)label };
    m_labels.insert_cols(m_labels.n_cols, newLabel);
    save();
}
