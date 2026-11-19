#include "detect_core.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <vector>

namespace block_detect
{
namespace
{

struct HsvRange
{
  int h_lo, s_lo, v_lo;
  int h_hi, s_hi, v_hi;
};

// OpenCV 的 H 是 0~179（不是 0~360），S / V 是 0~255。
// 红色跨 0 度，所以要两段区间拼起来。
const std::vector<HsvRange> & rangesFor(const std::string & color)
{
  static const std::vector<HsvRange> kRed = {
    {0, 90, 60, 10, 255, 255},
    {170, 90, 60, 179, 255, 255},
  };
  static const std::vector<HsvRange> kGreen = {{35, 90, 60, 90, 255, 255}};
  static const std::vector<HsvRange> kBlue = {{90, 90, 60, 130, 255, 255}};
  static const std::vector<HsvRange> kEmpty;

  if (color == "red") {
    return kRed;
  }
  if (color == "green") {
    return kGreen;
  }
  if (color == "blue") {
    return kBlue;
  }
  return kEmpty;
}

std::string trim(const std::string & text)
{
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

}  // namespace

std::string normalizeColor(const std::string & input)
{
  std::string key = trim(input);
  for (char & c : key) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  // 英文
  if (key == "red" || key == "r") {
    return "red";
  }
  if (key == "green" || key == "g") {
    return "green";
  }
  if (key == "blue" || key == "b") {
    return "blue";
  }
  // 中文
  if (key == "红" || key == "红色" || key == "红块" || key == "红方块") {
    return "red";
  }
  if (key == "绿" || key == "绿色" || key == "绿块" || key == "绿方块") {
    return "green";
  }
  if (key == "蓝" || key == "蓝色" || key == "蓝块" || key == "蓝方块") {
    return "blue";
  }
  return "";
}

BlockBox detectBlock(const cv::Mat & bgr, const std::string & color)
{
  BlockBox box;
  box.color = color;

  const std::vector<HsvRange> & ranges = rangesFor(color);
  if (bgr.empty() || ranges.empty()) {
    return box;
  }

  cv::Mat hsv;
  cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

  // 1. 按 HSV 阈值抠出目标颜色的像素
  cv::Mat mask = cv::Mat::zeros(hsv.size(), CV_8UC1);
  for (const HsvRange & r : ranges) {
    cv::Mat part;
    cv::inRange(
      hsv, cv::Scalar(r.h_lo, r.s_lo, r.v_lo), cv::Scalar(r.h_hi, r.s_hi, r.v_hi), part);
    cv::bitwise_or(mask, part, mask);
  }

  // 2. 先去小噪点（开运算），再把方块内部的空洞补上（闭运算）
  const cv::Mat kernel_small =
    cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
  const cv::Mat kernel_big =
    cv::getStructuringElement(cv::MORPH_RECT, cv::Size(9, 9));
  cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel_small);
  cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel_big);

  box.pixels = static_cast<uint32_t>(cv::countNonZero(mask));

  // 3. 取面积最大的轮廓当作方块
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
  if (contours.empty()) {
    return box;
  }

  const auto largest = std::max_element(
    contours.begin(), contours.end(),
    [](const std::vector<cv::Point> & a, const std::vector<cv::Point> & b) {
      return cv::contourArea(a) < cv::contourArea(b);
    });

  // 4. 面积太小说明只是噪声，不算识别成功
  const double min_area = bgr.total() * 0.002;  // 整幅图的 0.2%
  if (cv::contourArea(*largest) < min_area) {
    return box;
  }

  const cv::Rect rect = cv::boundingRect(*largest);
  box.found = true;
  box.x = rect.x;
  box.y = rect.y;
  box.width = rect.width;
  box.height = rect.height;
  return box;
}

cv::Mat drawBlockBox(const cv::Mat & bgr, const BlockBox & box)
{
  cv::Mat canvas = bgr.clone();

  if (!box.found) {
    const std::string text = "no " + box.color + " block found";
    cv::putText(
      canvas, text, cv::Point(12, 28), cv::FONT_HERSHEY_SIMPLEX, 0.7,
      cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    return canvas;
  }

  // 红色框（OpenCV 是 BGR，(0,0,255) 才是纯红），线宽 2，和题目给的样图一致
  const cv::Rect rect(box.x, box.y, box.width, box.height);
  cv::rectangle(canvas, rect, cv::Scalar(0, 0, 255), 2, cv::LINE_8);

  const std::string label =
    box.color + " " + std::to_string(box.x) + "," + std::to_string(box.y) + " " +
    std::to_string(box.width) + "x" + std::to_string(box.height);

  // 文字默认写在框上方；贴近画面顶部时改写到框下方，避免被裁掉
  int baseline = 0;
  const double font_scale = 0.55;
  cv::Size text_size =
    cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, font_scale, 2, &baseline);
  // 图太窄时只写颜色名，保证文字不越界
  std::string shown = label;
  if (text_size.width > canvas.cols - 24) {
    shown = box.color;
  }
  // 统一写在画面左上角，既不会被框挡住，也永远不会越界
  cv::putText(
    canvas, shown, cv::Point(12, 30), cv::FONT_HERSHEY_SIMPLEX, font_scale,
    cv::Scalar(0, 0, 255), 2, cv::LINE_AA);

  return canvas;
}

}  // namespace block_detect
