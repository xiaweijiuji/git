# 一次方块识别的结果（话题 /detect/result 上用）
std_msgs/Header header

# 目标颜色：red / green / blue
string color

# 是否找到了该颜色的方块
bool found

# 方块外接矩形（像素，左上角为原点，y 轴向下）
int32 x
int32 y
int32 width
int32 height

# 命中的像素个数，可用于判断识别质量
uint32 pixels
