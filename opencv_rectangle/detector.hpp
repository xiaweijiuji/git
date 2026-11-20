#pragma once

#include <opencv2/core.hpp>

#include <vector>

// 检测一张图里的所有矩形。
// 每个矩形用 4 个点表示，图像坐标（y 向下），已排成 左上 → 右上 → 右下 → 左下。
std::vector<std::vector<cv::Point2f>> detectRects(const cv::Mat& bgr);
