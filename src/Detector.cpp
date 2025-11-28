#include "Detector.h"
#include "AppleUtils.h"
#include <QtConcurrent>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>

static const int TARGET_SIZE = 640;
static const float CONF_THRESHOLD = 0.45f;
static const float NMS_THRESHOLD = 0.5f;

// --- Helpers for NMS ---
static float intersection_area(const Object& a, const Object& b) {
    cv::Rect_<float> inter = a.rect & b.rect;
    return inter.area();
}
static void qsort_descent_inplace(std::vector<Object>& faceobjects) {
    if (faceobjects.empty()) return;
    std::sort(faceobjects.begin(), faceobjects.end(), [](const Object& a, const Object& b) {
        return a.prob > b.prob;
    });
}
static void nms_sorted_bboxes(const std::vector<Object>& faceobjects, std::vector<int>& picked, float nms_threshold) {
    picked.clear();
    const int n = faceobjects.size();
    std::vector<float> areas(n);
    for (int i = 0; i < n; i++) areas[i] = faceobjects[i].rect.area();
    for (int i = 0; i < n; i++) {
        const Object& a = faceobjects[i];
        int keep = 1;
        for (int j = 0; j < (int)picked.size(); j++) {
            const Object& b = faceobjects[picked[j]];
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
            if (inter_area / union_area > nms_threshold) keep = 0;
        }
        if (keep) picked.push_back(i);
    }
}

Detector::Detector(QObject *parent) : QObject(parent) {
    // Копируем модель из ресурсов во временную папку (NCNN не читает из qrc)
    QString paramPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/yolo11n.param";
    QString binPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/yolo11n.bin";

    if (QFile::exists(":/models/yolo11n.param")) {
        QFile::remove(paramPath); QFile::remove(binPath);
        QFile::copy(":/models/yolo11n.param", paramPath);
        QFile::copy(":/models/yolo11n.bin", binPath);
        QFile::setPermissions(paramPath, QFile::ReadOwner | QFile::WriteOwner);
        QFile::setPermissions(binPath, QFile::ReadOwner | QFile::WriteOwner);
    }

    if (m_net.load_param(paramPath.toStdString().c_str()) == 0 &&
        m_net.load_model(binPath.toStdString().c_str()) == 0) {
        qDebug() << "Detector: NCNN loaded!";
    } else {
        qDebug() << "Detector: ERROR loading NCNN! Check assets.";
    }
}

Detector::~Detector() { m_net.clear(); }

void Detector::analyzeFromPath(const QString &imagePath) {
    QImage img(imagePath);
    if (img.isNull()) {
        emit analysisError("Не удалось загрузить изображение");
        return;
    }
    analyze(img);
}

void Detector::analyzeTestImage(int imageType) {
    QString resourcePath = (imageType == 0) ? ":/images/test_apple_good.jpg" : ":/images/test_apple_bad.jpg";

    // Копируем из ресурсов во временную папку
    QString tmpPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                      QString("/test_apple_%1.jpg").arg(imageType);

    QFile::remove(tmpPath);
    if (QFile::copy(resourcePath, tmpPath)) {
        QFile::setPermissions(tmpPath, QFile::ReadOwner | QFile::WriteOwner);
        QImage img(tmpPath);
        if (!img.isNull()) {
            qDebug() << "Detector: Loaded test image" << imageType << "size:" << img.width() << "x" << img.height();
            analyze(img);
        } else {
            emit analysisError("Не удалось загрузить тестовое изображение");
        }
    } else {
        emit analysisError("Тестовое изображение не найдено в ресурсах");
    }
}

void Detector::analyze(const QImage &img) {
    QtConcurrent::run([=]() {
        cv::Mat cvImg = AppleUtils::QImageToCvMat(img);
        if (cvImg.empty()) {
            QMetaObject::invokeMethod(const_cast<Detector*>(this), "analysisError",
                Qt::QueuedConnection, Q_ARG(QString, "Не удалось конвертировать изображение"));
            return;
        }

        int img_w = cvImg.cols;
        int img_h = cvImg.rows;

        // 1. Preprocess
        ncnn::Mat in = ncnn::Mat::from_pixels_resize(cvImg.data, ncnn::Mat::PIXEL_BGR2RGB, img_w, img_h, TARGET_SIZE, TARGET_SIZE);
        const float norm_vals[3] = {1/255.f, 1/255.f, 1/255.f};
        in.substract_mean_normalize(0, norm_vals);

        ncnn::Extractor ex = m_net.create_extractor();
        ex.input("images", in);

        ncnn::Mat out0, out1;
        ex.extract("output0", out0); // Detection
        ex.extract("output1", out1); // Mask Protos

        // 2. Parse YOLOv8/11 Output
        std::vector<Object> proposals;
        const int num_grid = out0.w;
        const int num_mask_proto = 32;

        qDebug() << "Detector: Parsing YOLO output, grid size:" << num_grid;

        for (int i = 0; i < num_grid; i++) {
            // output0 shape [dim, 8400]. 4=score
            float score = out0.row(4)[i];
            if (score > CONF_THRESHOLD) {
                Object obj;
                obj.label = 0;
                obj.prob = score;

                float x = out0.row(0)[i];
                float y = out0.row(1)[i];
                float w = out0.row(2)[i];
                float h = out0.row(3)[i];

                float x0 = x - w * 0.5f;
                float y0 = y - h * 0.5f;

                float scale_x = (float)img_w / TARGET_SIZE;
                float scale_y = (float)img_h / TARGET_SIZE;

                obj.rect.x = x0 * scale_x;
                obj.rect.y = y0 * scale_y;
                obj.rect.width = w * scale_x;
                obj.rect.height = h * scale_y;

                obj.mask_feat.resize(num_mask_proto);
                for (int k = 0; k < num_mask_proto; k++) obj.mask_feat[k] = out0.row(5 + k)[i];

                proposals.push_back(obj);
            }
        }

        // 3. NMS
        qsort_descent_inplace(proposals);
        std::vector<int> picked;
        nms_sorted_bboxes(proposals, picked, NMS_THRESHOLD);

        qDebug() << "Detector: Found" << proposals.size() << "proposals," << picked.size() << "after NMS";

        if (!picked.empty()) {
            Object& best = proposals[picked[0]];

            // 4. Decode Mask
            decode_mask(out1, img_w, img_h, ncnn::Mat(32, (void*)best.mask_feat.data()), best.mask);

            // 5. Remove Background (Black BG)
            cv::Mat mask_bin;
            if (best.mask.size() != cvImg.size()) cv::resize(best.mask, best.mask, cvImg.size());

            best.mask.convertTo(mask_bin, CV_8U, 255.0);
            cv::threshold(mask_bin, mask_bin, 127, 255, cv::THRESH_BINARY);

            cv::Mat maskedImg;
            cv::bitwise_and(cvImg, cvImg, maskedImg, mask_bin);

            // 6. Crop & Classify
            cv::Rect roi = best.rect;
            roi.x = std::max(0, roi.x); roi.y = std::max(0, roi.y);
            roi.width = std::min(img_w - roi.x, roi.width);
            roi.height = std::min(img_h - roi.y, roi.height);

            if (roi.area() > 0) {
                cv::Mat appleCrop = maskedImg(roi);
                arma::rowvec features = AppleUtils::extractFeatures(appleCrop);
                int result = m_brain.classify(features);

                const_cast<Detector*>(this)->m_lastFeatures = features;

                // Старый сигнал для совместимости
                QMetaObject::invokeMethod(const_cast<Detector*>(this), "resultReady",
                    Qt::QueuedConnection,
                    Q_ARG(float, (float)roi.x / img_w),
                    Q_ARG(float, (float)roi.y / img_h),
                    Q_ARG(float, (float)roi.width / img_w),
                    Q_ARG(float, (float)roi.height / img_h),
                    Q_ARG(int, result));

                // Новый сигнал для QML
                QString quality = (result == 1) ? "Хорошее 🍏" : "Плохое 🍎";
                QMetaObject::invokeMethod(const_cast<Detector*>(this), "analysisComplete",
                    Qt::QueuedConnection,
                    Q_ARG(QString, quality),
                    Q_ARG(float, best.prob));
                return;
            }
        }

        // Ничего не нашли
        QMetaObject::invokeMethod(const_cast<Detector*>(this), "analysisError",
            Qt::QueuedConnection,
            Q_ARG(QString, "Яблоко не обнаружено на изображении"));
    });
}

void Detector::fixMistake(bool isActuallyGood) {
    if (m_lastFeatures.n_elem > 0) m_brain.learn(m_lastFeatures, isActuallyGood ? 1 : 0);
}

void Detector::decode_mask(const ncnn::Mat& mask_proto, int img_w, int img_h, const ncnn::Mat& mask_feat, cv::Mat& mask_out) {
    int proto_h = mask_proto.h;
    int proto_w = mask_proto.w;
    int channels = mask_proto.c;
    ncnn::Mat mat_out(proto_w, proto_h, 1);
    mat_out.fill(0.f);
    for (int c = 0; c < channels; c++) {
        const float* proto_ptr = mask_proto.channel(c);
        float coeff = mask_feat[c];
        float* out_ptr = mat_out;
        for (int i = 0; i < proto_h * proto_w; i++) out_ptr[i] += proto_ptr[i] * coeff;
    }
    float* ptr = mat_out;
    for (int i = 0; i < proto_h * proto_w; i++) ptr[i] = 1.f / (1.f + exp(-ptr[i]));
    cv::Mat mask_small(proto_h, proto_w, CV_32FC1, mat_out.data);
    cv::resize(mask_small, mask_out, cv::Size(img_w, img_h));
}
