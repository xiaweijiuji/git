"""一键启动：图像发布节点 + 识别节点。

用法：
    ros2 launch block_detect_cpp demo.launch.py                # 启动后等控制台再指定颜色
    ros2 launch block_detect_cpp demo.launch.py color:=green   # 启动就直接识别绿色
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    color = LaunchConfiguration('color')

    return LaunchDescription([
        DeclareLaunchArgument(
            'color',
            default_value='',
            description='启动时先识别的颜色（red / green / blue），留空则等控制台指定'),
        Node(
            package='block_detect_cpp',
            executable='image_publisher',
            name='image_publisher',
            output='screen',
        ),
        Node(
            package='block_detect_cpp',
            executable='block_recognizer',
            name='recognition_node',
            output='screen',
            parameters=[{'initial_color': color}],
        ),
    ])
