#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/compressed_image.hpp"
#include "sensor_msgs/msg/image.hpp"

#include "block_detect_cpp/msg/block_result.hpp"
#include "block_detect_cpp/srv/detect_block.hpp"
#include "detect_core.hpp"

using DetectBlock = block_detect_cpp::srv::DetectBlock;
using BlockResult = block_detect_cpp::msg::BlockResult;

// 节点 2：订阅图像（话题通信）→ 按请求的颜色识别方块（服务通信）
//        → 发布带红框的标注图（话题通信），供 Foxglove 显示
class BlockRecognizer : public rclcpp::Node
{
public:
  BlockRecognizer()
  : Node("recognition_node")
  {
    image_topic_ = declare_parameter<std::string>("image_topic", "/camera/image_raw");
    compressed_topic_ = declare_parameter<std::string>("compressed_topic", "/camera/compressed");
    result_topic_ = declare_parameter<std::string>("result_topic", "/detect/result");
    service_name_ = declare_parameter<std::string>("service_name", "/detect_block");
    jpeg_quality_ = declare_parameter<int>("jpeg_quality", 90);
    const std::string initial_color = declare_parameter<std::string>("initial_color", "");

    subscription_ = create_subscription<sensor_msgs::msg::Image>(
      image_topic_, 10, std::bind(&BlockRecognizer::onImage, this, std::placeholders::_1));
    compressed_publisher_ =
      create_publisher<sensor_msgs::msg::CompressedImage>(compressed_topic_, 10);
    result_publisher_ = create_publisher<BlockResult>(result_topic_, 10);

    service_ = create_service<DetectBlock>(
      service_name_,
      std::bind(
        &BlockRecognizer::onDetect, this, std::placeholders::_1, std::placeholders::_2));

    RCLCPP_INFO(this->get_logger(), "订阅图像: %s", image_topic_.c_str());
    RCLCPP_INFO(this->get_logger(), "标注图发布到: %s", compressed_topic_.c_str());
    RCLCPP_INFO(this->get_logger(), "识别结果发布到: %s", result_topic_.c_str());
    RCLCPP_INFO(this->get_logger(), "识别服务: %s", service_name_.c_str());

    const std::string color = block_detect::normalizeColor(initial_color);
    if (!color.empty()) {
      target_color_ = color;
      has_target_ = true;
      RCLCPP_INFO(this->get_logger(), "启动即识别颜色: %s", target_color_.c_str());
    }
  }

private:
  // 收到一帧图像（话题通信的订阅端）
  void onImage(const sensor_msgs::msg::Image::SharedPtr msg)
  {
    if (msg->encoding != "bgr8" || msg->step != msg->width * 3) {
      RCLCPP_WARN_ONCE(this->get_logger(), "只支持 bgr8 编码的图，当前是 %s", msg->encoding.c_str());
      return;
    }

    frame_ = cv::Mat(
               static_cast<int>(msg->height), static_cast<int>(msg->width), CV_8UC3,
               const_cast<unsigned char *>(msg->data.data()), msg->step)
               .clone();

    if (!first_frame_logged_.exchange(true)) {
      RCLCPP_INFO(
        this->get_logger(), "收到第一帧图像: %d x %d", frame_.cols, frame_.rows);
    }

    if (has_target_) {
      last_box_ = block_detect::detectBlock(frame_, target_color_);
      publishAnnotated();
      publishResult();
    } else {
      publishAnnotated();  // 还没指定颜色：先发原图，Foxglove 里能看到画面
    }
  }

  // 服务回调：控制台指定颜色后走这里
  void onDetect(
    const std::shared_ptr<DetectBlock::Request> request,
    std::shared_ptr<DetectBlock::Response> response)
  {
    const std::string raw = request->color;
    const std::string color = block_detect::normalizeColor(raw);
    RCLCPP_INFO(this->get_logger(), "收到识别请求，目标颜色: %s", raw.c_str());

    if (color.empty()) {
      response->success = false;
      response->color = raw;
      response->message = "不支持的颜色 \"" + raw + "\"，可用 red / green / blue";
      RCLCPP_WARN(this->get_logger(), "%s", response->message.c_str());
      return;
    }

    if (frame_.empty()) {
      response->success = false;
      response->color = color;
      response->message = "还没有收到图像帧，请等 image_publisher 启动后再试";
      RCLCPP_WARN(this->get_logger(), "%s", response->message.c_str());
      return;
    }

    target_color_ = color;
    has_target_ = true;
    last_box_ = block_detect::detectBlock(frame_, target_color_);

    response->color = color;
    response->success = last_box_.found;
    response->x = last_box_.x;
    response->y = last_box_.y;
    response->width = last_box_.width;
    response->height = last_box_.height;
    response->pixels = last_box_.pixels;
    response->message = last_box_.found ? "" : ("画面里没有找到 " + color + " 方块");

    if (last_box_.found) {
      RCLCPP_INFO(
        this->get_logger(), "识别结果: 颜色=%s x=%d y=%d w=%d h=%d",
        color.c_str(), last_box_.x, last_box_.y, last_box_.width, last_box_.height);
    } else {
      RCLCPP_WARN(this->get_logger(), "%s", response->message.c_str());
    }

    publishAnnotated();
    publishResult();
  }

  // 发布带红框的标注图（话题通信的发布端，Foxglove 里看这个）
  void publishAnnotated()
  {
    if (frame_.empty()) {
      return;
    }

    const cv::Mat canvas =
      has_target_ ? block_detect::drawBlockBox(frame_, last_box_) : frame_;

    std::vector<uchar> buffer;
    const std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, jpeg_quality_};
    if (!cv::imencode(".jpg", canvas, buffer, params)) {
      RCLCPP_ERROR(this->get_logger(), "JPEG 编码失败");
      return;
    }

    sensor_msgs::msg::CompressedImage msg;
    msg.header.stamp = now();
    msg.header.frame_id = "camera";
    msg.format = "jpeg";
    msg.data = buffer;
    compressed_publisher_->publish(msg);
  }

  // 发布识别结果（自定义消息 BlockResult）
  void publishResult()
  {
    BlockResult msg;
    msg.header.stamp = now();
    msg.header.frame_id = "camera";
    msg.color = target_color_;
    msg.found = last_box_.found;
    msg.x = last_box_.x;
    msg.y = last_box_.y;
    msg.width = last_box_.width;
    msg.height = last_box_.height;
    msg.pixels = last_box_.pixels;
    result_publisher_->publish(msg);
  }

  std::string image_topic_;
  std::string compressed_topic_;
  std::string result_topic_;
  std::string service_name_;
  int jpeg_quality_ = 90;

  cv::Mat frame_;
  block_detect::BlockBox last_box_;
  std::string target_color_;
  bool has_target_ = false;
  std::atomic<bool> first_frame_logged_{false};

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr subscription_;
  rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr compressed_publisher_;
  rclcpp::Publisher<BlockResult>::SharedPtr result_publisher_;
  rclcpp::Service<DetectBlock>::SharedPtr service_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<BlockRecognizer>());
  rclcpp::shutdown();
  return 0;
}
