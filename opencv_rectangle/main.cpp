#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "detector.hpp"
#include "output.hpp"

namespace fs = std::filesystem;

using std::cout;
using std::endl;
using std::string;
using std::vector;

static bool hasPngExtension(const string& path) {
    string ext = fs::path(path).extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".png";
}

// 升序排列（数字序排序）
// 若有非数字字符，退回字典序排列
static long numericStem(const fs::path& p) {
    const string stem = p.stem().string();
    if (stem.empty()) return -1;
    for (const char c : stem) {
        if (c < '0' || c > '9') return -1;
    }
    try {
        return std::stol(stem);
    } catch (...) {
        return -1;
    }
}

static bool fileOrderLess(const fs::path& a, const fs::path& b) {
    const long na = numericStem(a);
    const long nb = numericStem(b);
    if (na >= 0 && nb >= 0) return na < nb;
    return a < b;
}

static fs::path projectDir() {
    std::error_code ec;
    fs::path dir = fs::read_symlink("/proc/self/exe", ec).parent_path();
    if (ec || dir.empty()) dir = fs::current_path();
    if (dir.filename() == "build") dir = dir.parent_path();
    return dir;
}

int main() {
    const fs::path base = projectDir();

    // 有 /input 就按考核约定走（/input 读、/out 写）；没有就用程序旁边的 input/、out/
    const bool useSystemDirs = fs::is_directory("/input");
    const fs::path inDir = useSystemDirs ? fs::path("/input") : base / "input";
    const fs::path outDir = useSystemDirs ? fs::path("/out") : base / "out";

    std::error_code ec;
    fs::create_directories(outDir, ec);
    if (!fs::exists(outDir) || !fs::is_directory(outDir)) {
        std::cerr << "错误：无法创建输出目录 " << outDir.string() << endl;
        return 1;
    }
    if (!fs::exists(inDir) || !fs::is_directory(inDir)) {
        std::cerr << "错误：输入目录不存在 " << inDir.string() << endl;
        return 1;
    }

    vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(inDir)) {
        if (!entry.is_regular_file()) continue;
        if (!hasPngExtension(entry.path().string())) continue;
        files.push_back(entry.path());
    }

    // 按数值升序排列文件名
    std::sort(files.begin(), files.end(), fileOrderLess);

    if (files.empty()) {
        std::cerr << "提示：" << inDir.string() << " 里没有 PNG 文件" << endl;
        return 0;
    }

    for (const auto& file : files) {
        cv::Mat image = cv::imread(file.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
            std::cerr << "警告：读不出图片 " << file.string() << "，跳过" << endl;
            continue;
        }

        const vector<vector<cv::Point2f>> rects = detectRects(image);

        // 画框：绿色 (0,255,0)，线宽 2。
        // 注意 OpenCV 的颜色顺序是 BGR，所以纯绿写成 Scalar(0, 255, 0)。
        // 最后一个参数用 LINE_8（普通直线）而不是 LINE_AA（抗锯齿），
        // 因为抗锯齿会把边缘像素混成半绿半底色，颜色就不纯了。
        for (const auto& rect : rects) {
            vector<cv::Point> polyline;
            for (const auto& p : rect) {
                polyline.push_back(cv::Point(static_cast<int>(std::lround(p.x)),
                                             static_cast<int>(std::lround(p.y))));
            }
            cv::polylines(image, polyline, true, cv::Scalar(0, 255, 0), 2, cv::LINE_8);
        }

        const fs::path outPath = outDir / file.filename();
        if (!cv::imwrite(outPath.string(), image)) {
            std::cerr << "警告：写不出图片 " << outPath.string() << endl;
        }

        printResult(file.filename().string(), rects, image.rows);
    }

    return 0;
}
