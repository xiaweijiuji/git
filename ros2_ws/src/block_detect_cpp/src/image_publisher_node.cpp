#include <chrono>
#include <memory>
#include <string>

#include <opencv2/imgcodecs.hpp>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"

using namespace std::chrono_literals;

// 节点 1：把样图当成"相机"画面，按固定频率发布到话题 /camera/image_raw
class ImagePublisher : public rclcpp::Node
{
public:
  ImagePublisher()
  : Node("image_publisher")
  {
    const std::string default_path =
      ament_index_cpp::get_package_share_directory("block_detect_cpp") + "/resource/cube.png";

    image_path_ = declare_parameter<std::string>("image_path", default_path);
    const double fps = declare_parameter<double>("fps", 10.0);
    const std::string topic = declare_parameter<std::string>("topic", "/camera/image_raw");

    image_ = cv::imread(image_path_, cv::IMREAD_COLOR);
    if (image_.empty()) {
      RCLCPP_ERROR(this->get_logger(), "读不到样图: %s", image_path_.c_str());
      throw std::runtime_error("读不到样图: " + image_path_);
    }

    RCLCPP_INFO(this->get_logger(), "图片路径: %s", image_path_.c_str());
    RCLCPP_INFO(
      this->get_logger(), "图片尺寸: %d x %d，发布到 %s，频率 %.1f Hz",
      image_.cols, image_.rows, topic.c_str(), fps);

    publisher_ = create_publisher<sensor_msgs::msg::Image>(topic, 10);

    const auto period = std::chrono::duration<double>(1.0 / (fps > 0.0 ? fps : 10.0));
    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::milliseconds>(period),
      std::bind(&ImagePublisher::onTimer, this));
  }

private:
  void onTimer()
  {
    sensor_msgs::msg::Image msg;
    msg.header.stamp = now();
    msg.header.frame_id = "camera";
    msg.height = static_cast<uint32_t>(image_.rows);
    msg.width = static_cast<uint32_t>(image_.cols);
    msg.encoding = "bgr8";
    msg.is_bigendian = false;
    msg.step = static_cast<uint32_t>(image_.cols * 3);

    // cv::Mat 是行优先、连续存放的，直接整块拷进 data 即可
    const size_t total = static_cast<size_t>(image_.rows) * msg.step;
    msg.data.assign(image_.data, image_.data + total);

    publisher_->publish(msg);

    if (++count_ % 100 == 0) {
      RCLCPP_INFO(this->get_logger(), "已发布 %d 帧", count_);
    }
  }

  std::string image_path_;
  cv::Mat image_;
  int count_ = 0;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<ImagePublisher>());
  } catch (const std::exception & e) {
    RCLCPP_FATAL(rclcpp::get_logger("image_publisher"), "%s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
