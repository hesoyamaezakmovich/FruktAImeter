#include "Detector.h"
#include "AppleUtils.h"
#include <QtConcurrent>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>

static const int TARGET_SIZE = 640;
static const float CONF_THRESHOLD = 0.25f;
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
    QString paramPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/yolo11n.param";
    QString binPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/yolo11n.bin";

    qDebug() << "Detector: Copying model to" << paramPath;

    if (QFile::exists(":/models/yolo11n.param")) {
        QFile::remove(paramPath); QFile::remove(binPath);
        bool p1 = QFile::copy(":/models/yolo11n.param", paramPath);
        bool p2 = QFile::copy(":/models/yolo11n.bin", binPath);
        QFile::setPermissions(paramPath, QFile::ReadOwner | QFile::WriteOwner);
        QFile::setPermissions(binPath, QFile::ReadOwner | QFile::WriteOwner);
        qDebug() << "Detector: Copy result param:" << p1 << "bin:" << p2;
    } else {
        qDebug() << "Detector: ERROR - model not found in resources!";
    }

    int ret1 = m_net.load_param(paramPath.toStdString().c_str());
    int ret2 = m_net.load_model(binPath.toStdString().c_str());
    
    if (ret1 == 0 && ret2 == 0) {
        qDebug() << "Detector: NCNN loaded successfully!";
    } else {
        qDebug() << "Detector: ERROR loading NCNN! param:" << ret1 << "bin:" << ret2;
    }
}

Detector::~Detector() { m_net.clear(); }

void Detector::analyzeFromPath(const QString &imagePath) {
    QString path = imagePath;
    if (path.startsWith("file://")) {
        path = path.mid(7);
    }
    
    qDebug() << "Detector: Loading image from" << path;
    
    QImage img(path);
    if (img.isNull()) {
        qDebug() << "Detector: Failed to load image!";
        emit analysisError("Не удалось загрузить изображение");
        return;
    }
    
    qDebug() << "Detector: Image loaded, size:" << img.width() << "x" << img.height();
    analyze(img);
}

void Detector::analyzeTestImage(int imageType) {
    QString resourcePath = (imageType == 0) ? ":/images/test_apple_good.jpg" : ":/images/test_apple_bad.jpg";

    qDebug() << "Detector: Loading test image" << imageType << "from" << resourcePath;

    QString tmpPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                      QString("/test_apple_%1.jpg").arg(imageType);

    QFile::remove(tmpPath);
    if (QFile::copy(resourcePath, tmpPath)) {
        QFile::setPermissions(tmpPath, QFile::ReadOwner | QFile::WriteOwner);
        QImage img(tmpPath);
        if (!img.isNull()) {
            qDebug() << "Detector: Test image loaded, size:" << img.width() << "x" << img.height();
            analyze(img);
        } else {
            emit analysisError("Не удалось загрузить тестовое изображение");
        }
    } else {
        qDebug() << "Detector: Resource not found:" << resourcePath;
        emit analysisError("Тестовое изображение не найдено в ресурсах");
    }
}

void Detector::analyze(const QImage &img) {
    QtConcurrent::run([=]() {
        cv::Mat cvImg = AppleUtils::QImageToCvMat(img);
        if (cvImg.empty()) {
            qDebug() << "Detector: Failed to convert QImage to cv::Mat";
            QMetaObject::invokeMethod(const_cast<Detector*>(this), "analysisError",
                Qt::QueuedConnection, Q_ARG(QString, "Не удалось конвертировать изображение"));
            return;
        }

        int img_w = cvImg.cols;
        int img_h = cvImg.rows;
        qDebug() << "Detector: Processing image" << img_w << "x" << img_h;

        // 1. Preprocess
        ncnn::Mat in = ncnn::Mat::from_pixels_resize(cvImg.data, ncnn::Mat::PIXEL_BGR2RGB, img_w, img_h, TARGET_SIZE, TARGET_SIZE);
        const float norm_vals[3] = {1/255.f, 1/255.f, 1/255.f};
        in.substract_mean_normalize(0, norm_vals);

        qDebug() << "Detector: Input prepared, running inference...";

        ncnn::Extractor ex = m_net.create_extractor();
        ex.set_light_mode(true);
        
        // Используем точные имена из .param файла
        int input_ret = ex.input("in0", in);
        if (input_ret != 0) {
            qDebug() << "Detector: ERROR setting input 'in0', ret:" << input_ret;
            goto fallback_classify;
        }
        qDebug() << "Detector: Input 'in0' set successfully";

        {
            ncnn::Mat out0;
            int extract_ret = ex.extract("out0", out0);
            if (extract_ret != 0) {
                qDebug() << "Detector: ERROR extracting 'out0', ret:" << extract_ret;
                goto fallback_classify;
            }

            qDebug() << "Detector: Output 'out0' shape - w:" << out0.w << "h:" << out0.h << "c:" << out0.c << "dims:" << out0.dims;

            // Анализируем формат выхода
            // YOLOv8/11 выход обычно: [num_features, 8400] где num_features = 4 + num_classes (+ 32 для seg)
            // Или может быть транспонирован: [8400, num_features]
            
            std::vector<Object> proposals;
            int num_proposals = 0;
            int num_features = 0;
            bool transposed = false;
            
            // Определяем ориентацию данных
            if (out0.w == 8400) {
                // Формат: [features, 8400] - стандартный YOLOv8
                num_proposals = out0.w;
                num_features = out0.h;
                transposed = false;
            } else if (out0.h == 8400) {
                // Формат: [8400, features] - транспонированный
                num_proposals = out0.h;
                num_features = out0.w;
                transposed = true;
            } else {
                // Попробуем как есть
                num_proposals = out0.w;
                num_features = out0.h;
                qDebug() << "Detector: Unknown output format, trying w as proposals";
            }
            
            int num_classes = num_features - 4;  // 4 = x,y,w,h
            if (num_classes > 32) {
                // Вероятно есть mask coefficients, уберём их
                num_classes = num_classes - 32;
            }
            if (num_classes < 1) num_classes = 1;
            
            qDebug() << "Detector: Parsing - proposals:" << num_proposals 
                     << "features:" << num_features 
                     << "classes:" << num_classes
                     << "transposed:" << transposed;

            float max_score_found = 0;
            int detections_above_threshold = 0;
            
            for (int i = 0; i < num_proposals; i++) {
                float x, y, w, h;
                float max_class_score = 0;
                int max_class_idx = 0;
                
                if (transposed) {
                    // [8400, features] - строки это proposals
                    const float* row = out0.row(i);
                    x = row[0];
                    y = row[1];
                    w = row[2];
                    h = row[3];
                    for (int c = 0; c < num_classes; c++) {
                        float score = row[4 + c];
                        if (score > max_class_score) {
                            max_class_score = score;
                            max_class_idx = c;
                        }
                    }
                } else {
                    // [features, 8400] - колонки это proposals
                    x = out0.row(0)[i];
                    y = out0.row(1)[i];
                    w = out0.row(2)[i];
                    h = out0.row(3)[i];
                    for (int c = 0; c < num_classes; c++) {
                        float score = out0.row(4 + c)[i];
                        if (score > max_class_score) {
                            max_class_score = score;
                            max_class_idx = c;
                        }
                    }
                }
                
                if (max_class_score > max_score_found) {
                    max_score_found = max_class_score;
                }
                
                if (max_class_score > CONF_THRESHOLD) {
                    detections_above_threshold++;
                    
                    Object obj;
                    obj.label = max_class_idx;
                    obj.prob = max_class_score;

                    float x0 = x - w * 0.5f;
                    float y0 = y - h * 0.5f;

                    float scale_x = (float)img_w / TARGET_SIZE;
                    float scale_y = (float)img_h / TARGET_SIZE;

                    obj.rect.x = x0 * scale_x;
                    obj.rect.y = y0 * scale_y;
                    obj.rect.width = w * scale_x;
                    obj.rect.height = h * scale_y;

                    proposals.push_back(obj);
                    
                    if (detections_above_threshold <= 5) {
                        qDebug() << "Detector: Detection" << detections_above_threshold
                                 << "class:" << max_class_idx 
                                 << "score:" << max_class_score
                                 << "bbox:" << obj.rect.x << obj.rect.y << obj.rect.width << obj.rect.height;
                    }
                }
            }

            qDebug() << "Detector: Max score found:" << max_score_found 
                     << "detections above threshold:" << detections_above_threshold;

            // NMS
            qsort_descent_inplace(proposals);
            std::vector<int> picked;
            nms_sorted_bboxes(proposals, picked, NMS_THRESHOLD);

            qDebug() << "Detector: After NMS:" << picked.size() << "objects";

            if (!picked.empty()) {
                Object& best = proposals[picked[0]];
                
                qDebug() << "Detector: Best detection - class" << best.label << "prob" << best.prob;

                cv::Rect roi;
                roi.x = std::max(0, (int)best.rect.x);
                roi.y = std::max(0, (int)best.rect.y);
                roi.width = std::min(img_w - roi.x, (int)best.rect.width);
                roi.height = std::min(img_h - roi.y, (int)best.rect.height);

                if (roi.width > 10 && roi.height > 10) {
                    cv::Mat appleCrop = cvImg(roi);
                    arma::rowvec features = AppleUtils::extractFeatures(appleCrop);
                    int result = m_brain.classify(features);

                    const_cast<Detector*>(this)->m_lastFeatures = features;

                    QString quality = (result == 1) ? "Хорошее 🍏" : "Плохое 🍎";
                    qDebug() << "Detector: Classification result:" << quality;
                    
                    QMetaObject::invokeMethod(const_cast<Detector*>(this), "analysisComplete",
                        Qt::QueuedConnection,
                        Q_ARG(QString, quality),
                        Q_ARG(float, best.prob));
                    return;
                }
            }
        }

        // Fallback: классифицируем всё изображение
        fallback_classify:
        qDebug() << "Detector: Fallback - classifying full image...";
        
        {
            arma::rowvec features = AppleUtils::extractFeatures(cvImg);
            if (features.n_elem > 0) {
                int result = m_brain.classify(features);
                const_cast<Detector*>(this)->m_lastFeatures = features;
                
                QString quality = (result == 1) ? "Хорошее 🍏" : "Плохое 🍎";
                qDebug() << "Detector: Fallback result:" << quality;
                
                QMetaObject::invokeMethod(const_cast<Detector*>(this), "analysisComplete",
                    Qt::QueuedConnection,
                    Q_ARG(QString, quality),
                    Q_ARG(float, 0.7f));
                return;
            }
        }

        QMetaObject::invokeMethod(const_cast<Detector*>(this), "analysisError",
            Qt::QueuedConnection,
            Q_ARG(QString, "Ошибка анализа изображения"));
    });
}

void Detector::fixMistake(bool isActuallyGood) {
    if (m_lastFeatures.n_elem > 0) {
        m_brain.learn(m_lastFeatures, isActuallyGood ? 1 : 0);
        qDebug() << "Detector: Model updated with label" << (isActuallyGood ? 1 : 0);
    }
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
