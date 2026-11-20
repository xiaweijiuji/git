#pragma once

#include <opencv2/core.hpp>

#include <string>
#include <vector>

// 按考核格式打印一张图的结果：
//   1.png: rect1 (120, 350); (300, 350); (300, 200); (120, 200)
//   2.png: null
// 坐标在这里从图像坐标翻成「左下角为原点、y 向上」。
void printResult(const std::string& fileName,
                 const std::vector<std::vector<cv::Point2f>>& rects,
                 int imageHeight);
