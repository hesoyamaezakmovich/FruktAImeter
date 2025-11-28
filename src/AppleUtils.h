#ifndef APPLEUTILS_H
#define APPLEUTILS_H

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <armadillo>
#include <QImage>

class AppleUtils {
public:
    // Конвертация QImage -> cv::Mat
    static cv::Mat QImageToCvMat(const QImage &inImage) {
        if (inImage.isNull()) return cv::Mat();

        cv::Mat mat(inImage.height(), inImage.width(), CV_8UC4,
                    const_cast<uchar*>(inImage.bits()),
                    static_cast<size_t>(inImage.bytesPerLine()));

        cv::Mat matNoAlpha;
        cv::cvtColor(mat, matNoAlpha, cv::COLOR_RGBA2BGR);
        return matNoAlpha.clone();
    }

    // Извлечение вектора признаков (22 числа)
    static arma::rowvec extractFeatures(const cv::Mat& inputImage) {
        if (inputImage.empty()) return arma::rowvec();

        cv::Mat resizedImg;
        cv::resize(inputImage, resizedImg, cv::Size(64, 64));

        cv::Mat hsvImage;
        cv::cvtColor(resizedImg, hsvImage, cv::COLOR_BGR2HSV);

        // 1. Статистика
        cv::Scalar mean, stddev;
        cv::meanStdDev(hsvImage, mean, stddev);

        // 2. Гистограммы
        int h_bins = 8, v_bins = 8;
        float h_range[] = { 0, 180 }; const float* h_histRange = { h_range };
        float v_range[] = { 0, 256 }; const float* v_histRange = { v_range };

        cv::Mat h_hist, v_hist;
        int ch_h[] = { 0 };
        int ch_v[] = { 2 };

        cv::calcHist(&hsvImage, 1, ch_h, cv::Mat(), h_hist, 1, &h_bins, &h_histRange);
        cv::calcHist(&hsvImage, 1, ch_v, cv::Mat(), v_hist, 1, &v_bins, &v_histRange);

        cv::normalize(h_hist, h_hist, 0, 1, cv::NORM_MINMAX);
        cv::normalize(v_hist, v_hist, 0, 1, cv::NORM_MINMAX);

        // Сборка вектора
        arma::rowvec features(22);
        int idx = 0;

        for(int i=0; i<3; ++i) features[idx++] = mean[i];
        for(int i=0; i<3; ++i) features[idx++] = stddev[i];
        for(int i=0; i<h_bins; ++i) features[idx++] = h_hist.at<float>(i);
        for(int i=0; i<v_bins; ++i) features[idx++] = v_hist.at<float>(i);

        return features;
    }
};
#endif
