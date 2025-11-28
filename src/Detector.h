#ifndef DETECTOR_H
#define DETECTOR_H

#include <QObject>
#include <QImage>
#include <QRectF>
#include <QString>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "net.h" // NCNN
#include "Brain.h"

struct Object {
    cv::Rect_<float> rect;
    int label;
    float prob;
    std::vector<float> mask_feat;
    cv::Mat mask;
};

class Detector : public QObject {
    Q_OBJECT
public:
    explicit Detector(QObject *parent = nullptr);
    ~Detector();

    Q_INVOKABLE void analyze(const QImage &img);
    Q_INVOKABLE void analyzeFromPath(const QString &imagePath);
    Q_INVOKABLE void analyzeTestImage(int imageType); // 0 = good, 1 = bad
    Q_INVOKABLE void fixMistake(bool isActuallyGood);

signals:
    void resultReady(float x, float y, float w, float h, int type);
    void analysisComplete(QString quality, float confidence);
    void analysisError(QString errorMessage);

private:
    Brain m_brain;
    arma::rowvec m_lastFeatures;
    ncnn::Net m_net;

    // Вспомогательные методы
    void decode_mask(const ncnn::Mat& mask_proto, int img_w, int img_h, const ncnn::Mat& mask_feat, cv::Mat& mask_out);
};

#endif
