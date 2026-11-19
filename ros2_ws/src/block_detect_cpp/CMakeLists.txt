cmake_minimum_required(VERSION 3.8)
project(block_detect_cpp)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

# 自定义消息 / 服务：话题通信发结果，服务通信收「要识别哪种颜色」
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/BlockResult.msg"
  "srv/DetectBlock.srv"
  DEPENDENCIES std_msgs
)

# 颜色检测的公共实现（三个可执行文件都要用）
add_library(detect_core STATIC src/detect_core.cpp)
target_include_directories(detect_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(detect_core PUBLIC ${OpenCV_LIBS})

# 节点 1：把样图当相机画面，按固定频率发到话题上
add_executable(image_publisher src/image_publisher_node.cpp)
ament_target_dependencies(image_publisher rclcpp sensor_msgs)
target_link_libraries(image_publisher detect_core)

# 节点 2：订阅图像、提供识别服务、发布标注后的图像和识别结果
add_executable(block_recognizer src/block_recognizer_node.cpp)
ament_target_dependencies(block_recognizer rclcpp std_msgs sensor_msgs)
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(block_recognizer detect_core "${cpp_typesupport_target}")

# 节点 3：在控制台输入颜色，调用识别服务
add_executable(color_client src/color_client_node.cpp)
ament_target_dependencies(color_client rclcpp)
target_link_libraries(color_client "${cpp_typesupport_target}")

install(TARGETS image_publisher block_recognizer color_client
  RUNTIME DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch resource
  DESTINATION share/${PROJECT_NAME})

ament_export_dependencies(rosidl_default_runtime)
ament_package()
