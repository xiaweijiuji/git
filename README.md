# rectangle

批量矩形检测程序。读 `input/` 里的 PNG，把绿框画到 `out/` 下的同名图片，并在终端按考核格式输出四个角点。

## 目录

```
rectangle/
├── input/          放待检测的 PNG（已经放了 12 张测试图）
├── out/            结果图，程序自动创建
├── main.cpp
├── CMakeLists.txt
├── build.sh
└── build/          编译产物
```

## 编译

```bash
cd ~/rectangle
./build.sh
```

或者：

```bash
g++ -O2 -std=c++17 main.cpp -o rectangle `pkg-config --cflags --libs opencv4`
```

## 运行

```bash
cd ~/rectangle
./build/rectangle
```

**输入输出目录自动二选一**：

- 如果机器上存在 `/input/`（考核环境），就读 `/input/`、写 `/out/`；
- 否则退回程序自己所在目录的 `input/`、`out/`（本地调试），在哪个目录执行都可以。

想在本机模拟考核环境：

```bash
sudo mkdir -p /input && sudo cp input/*.png /input/
./build/rectangle          # 这次会读 /input，结果写到 /out
```

## 输出格式

```
01_easy_axis.png: rect1 (189, 318); (449, 320); (451, 160); (191, 158)
04_easy_none.png: null
```

## 约定

- 坐标原点在图片左下角，x 向右、y 向上，输出用 `y = (H - 1) - y_图像`；
- 角点顺序：左上 → 右上 → 右下 → 左下（按图像方位）；
- 文件名按数值序排（`1.png` → `2.png` → `10.png`）；
- 画框颜色纯绿 `(0, 255, 0)`，线宽 2。
