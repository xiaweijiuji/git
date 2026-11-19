#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "block_detect_cpp/srv/detect_block.hpp"

using namespace std::chrono_literals;
using DetectBlock = block_detect_cpp::srv::DetectBlock;

// 节点 3：在控制台上输入颜色 -> 调用识别服务 -> 打印识别结果
class ColorClient : public rclcpp::Node
{
public:
  ColorClient()
  : Node("color_client")
  {
    service_name_ = declare_parameter<std::string>("service_name", "/detect_block");
    client_ = create_client<DetectBlock>(service_name_);
  }

  bool waitService()
  {
    std::cout << "等待识别服务 " << service_name_ << " ..." << std::endl;
    if (!client_->wait_for_service(10s)) {
      std::cout << "服务没有出现，请先启动 recognition_node" << std::endl;
      return false;
    }
    return true;
  }

  bool call(const std::string & color)
  {
    auto request = std::make_shared<DetectBlock::Request>();
    request->color = color;

    auto future = client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(shared_from_this(), future, 5s) !=
      rclcpp::FutureReturnCode::SUCCESS)
    {
      std::cout << "调用服务超时" << std::endl;
      return false;
    }

    const auto response = future.get();
    if (!response->success) {
      std::cout << "识别失败：" << response->message << std::endl;
      return false;
    }

    std::cout << "识别成功：" << response->color << " 方块 x=" << response->x
              << " y=" << response->y << " w=" << response->width
              << " h=" << response->height << "（命中像素 " << response->pixels << "）"
              << std::endl;
    return true;
  }

private:
  std::string service_name_;
  rclcpp::Client<DetectBlock>::SharedPtr client_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<ColorClient>();
  if (!node->waitService()) {
    rclcpp::shutdown();
    return 1;
  }

  // 带参数时只识别一次：ros2 run block_detect_cpp color_client green
  if (argc > 1) {
    const bool ok = node->call(argv[1]);
    rclcpp::shutdown();
    return ok ? 0 : 1;
  }

  // 不带参数时进入交互模式
  std::cout << "输入要识别的颜色（red / green / blue，也可以用 红 / 绿 / 蓝），"
            << "输入 q 退出" << std::endl;

  std::string line;
  while (rclcpp::ok()) {
    std::cout << "颜色> " << std::flush;
    if (!std::getline(std::cin, line)) {
      break;
    }
    if (line.empty()) {
      continue;
    }
    if (line == "q" || line == "quit" || line == "exit") {
      break;
    }
    node->call(line);
  }

  rclcpp::shutdown();
  return 0;
}
