#include "output.hpp"

#include <cmath>
#include <iostream>

using std::cout;
using std::endl;

// ============================================================================
//  第 6 步：把一张图的结果打印成考核要求的格式
//
//  形如：  1.png: rect1 (120, 350); (300, 350); (300, 200); (120, 200)
//          2.png: null
//
//  注意 y 要在这里翻过来：y_输出 = (H - 1) - y_图像
// ============================================================================
void printResult(const std::string& fileName,
                 const std::vector<std::vector<cv::Point2f>>& rects,
                 int imageHeight) {
    cout << fileName << ":";

    if (rects.empty()) {
        cout << " null" << endl;
        return;
    }

    for (size_t i = 0; i < rects.size(); ++i) {
        if (i > 0) cout << " |";          // 多个矩形之间用 " | " 分隔
        cout << " rect" << (i + 1);       // rect1、rect2 ...

        for (size_t k = 0; k < 4; ++k) {
            if (k > 0) cout << ";";       // 同一矩形内，点与点之间用 "; " 分隔

            // 坐标取最近的整数像素
            const int x = static_cast<int>(std::lround(rects[i][k].x));
            const int y = imageHeight - 1 - static_cast<int>(std::lround(rects[i][k].y));

            cout << " (" << x << ", " << y << ")";
        }
    }
    cout << endl;
}
