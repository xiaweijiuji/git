#ifndef BLOCK_DETECT_CPP__DETECT_CORE_HPP_
#define BLOCK_DETECT_CPP__DETECT_CORE_HPP_

#include <opencv2/core.hpp>

#include <cstdint>
#include <string>

namespace block_detect
{

// 一个方块的外接矩形（像素坐标，原点在左上角，y 轴向下）
struct BlockBox
{
  bool found = false;
  std::string color;     // 统一后的颜色名：red / green / blue
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  uint32_t pixels = 0;   // 命中的像素个数
};

// 把用户输入的写法统一成 red / green / blue。
// 支持 "red"/"RED"/"红"/"红色" 等写法；不认识的颜色返回空字符串。
std::string normalizeColor(const std::string & input);

// 在 bgr 图上找指定颜色方块的外接矩形。
// color 必须是 normalizeColor() 的返回值；找不到时返回值里 found = false。
BlockBox detectBlock(const cv::Mat & bgr, const std::string & color);

// 在图上画出结果框和标注文字，返回画好的新图（原图不动）。
// 没有找到时不画框，只在左上角写一行提示。
cv::Mat drawBlockBox(const cv::Mat & bgr, const BlockBox & box);

}  // namespace block_detect

#endif  // BLOCK_DETECT_CPP__DETECT_CORE_HPP_
