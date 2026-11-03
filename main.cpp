#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using std::cout;
using std::endl;
using std::string;
using std::vector;

// 目标的最小面积（单位：像素²）。比它小的当作噪点、文字笔画、灰尘。
const double MIN_AREA = 1500.0;

// 目标的最短边（像素）。极细的条状物虽然几何上也算矩形，
// 但更可能是图片边框、表格线这类干扰，这里不要。
const double MIN_SIDE = 20.0;

// 「轮廓面积 ÷ 最小外接旋转矩形面积」的下限。
// 一个真正的矩形，这两个面积几乎相等（比值接近 1）；
// 如果轮廓是 L 形、三角形，比值就会明显小于 1。
const double FILL_RATIO_MIN = 0.85;

// 相邻两条边夹角偏离 90° 的允许程度（用余弦值表示）。
// |cos| ≤ 0.30 大约相当于允许 ±17° 的误差。
const double CORNER_COS_MAX = 0.30;

// 多边形简化的精度：允许简化后的折线偏离原轮廓 2% 的周长。
// 数值越小越「贴身」（点更多），越大越「粗糙」（点更少）。
const double APPROX_EPS_RATIO = 0.02;

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
// 不懂
static bool fileOrderLess(const fs::path& a, const fs::path& b) {
    const long na = numericStem(a);
    const long nb = numericStem(b);
    if (na >= 0 && nb >= 0) return na < nb;
    return a < b;
}

// 求四个点的中心（四个点加起来除以 4）。
static cv::Point2f centerOf(const vector<cv::Point2f>& pts) {
    cv::Point2f c(0.0f, 0.0f);
    for (const auto& p : pts) c += p;
    c *= 1.0f / static_cast<float>(pts.size());
    return c;
}

// 把 4 个角点排成固定顺序：左上 → 右上 → 右下 → 左下。
static vector<cv::Point2f> orderCorners(vector<cv::Point2f> pts) {
    cv::Point2f c = centerOf(pts);

    std::sort(pts.begin(), pts.end(), [&c](const cv::Point2f& a, const cv::Point2f& b) {
        return std::atan2(a.y - c.y, a.x - c.x) < std::atan2(b.y - c.y, b.x - c.x);
    });

    int first = 0;
    for (int i = 1; i < 4; ++i) {
        if (pts[i].x + pts[i].y < pts[first].x + pts[first].y) first = i;
    }
    std::rotate(pts.begin(), pts.begin() + first, pts.end());

    return pts;
}

// 判断这个四边形是不是“几乎占满整幅图”。
static bool isNearFullFrame(const vector<cv::Point2f>& pts, const cv::Size& imgSize) {
    cv::Rect box = cv::boundingRect(pts);
    if (box.width >= 0.95 * imgSize.width) return true;
    if (box.height >= 0.95 * imgSize.height) return true;
    return static_cast<double>(box.width) * box.height >=
           0.90 * static_cast<double>(imgSize.width) * imgSize.height;
}

static bool looksLikeRect(const vector<cv::Point>& quad, const cv::Size& imgSize) {
    // 条件 1：简化后必须正好剩 4 个点。
    if (quad.size() != 4) return false;
    
    // 条件 2：必须是凸四边形。
    if (!cv::isContourConvex(quad)) return false;
    
    // 条件 3：面积要够大，太小的当作噪点。
    const double area = cv::contourArea(quad);
    if (area < MIN_AREA) return false;
    
    // 条件 4：轮廓面积要接近「最小外接旋转矩形」的面积。
    //         矩形：比值≈1；三角形、L 形：比值明显小。
    const cv::RotatedRect rr = cv::minAreaRect(quad);
    const double rrArea = static_cast<double>(rr.size.width) * rr.size.height;
    if (rrArea <= 1.0) return false;
    if (area / rrArea < FILL_RATIO_MIN) return false;

    // 条件 5：边长不能太小、也不能太细长。    
    const double shortSide = std::min(rr.size.width, rr.size.height);
    const double longSide = std::max(rr.size.width, rr.size.height);
    if (shortSide < MIN_SIDE) return false;
    if (longSide / shortSide > 10.0) return false;

    // 条件 6：四个内角都要接近 90°。
    for (int i = 0; i < 4; ++i) {
        const cv::Point2f cur(static_cast<float>(quad[i].x), static_cast<float>(quad[i].y));
        const cv::Point2f prev(static_cast<float>(quad[(i + 3) % 4].x),
                               static_cast<float>(quad[(i + 3) % 4].y));
        const cv::Point2f next(static_cast<float>(quad[(i + 1) % 4].x),
                               static_cast<float>(quad[(i + 1) % 4].y));
        const cv::Point2f a = prev - cur;
        const cv::Point2f b = next - cur;
        const double cosv = a.dot(b) / (cv::norm(a) * cv::norm(b) + 1e-9);
        if (std::fabs(cosv) > CORNER_COS_MAX) return false;
    }

    // 条件 7：不能几乎占满整幅图
    vector<cv::Point2f> pts(quad.begin(), quad.end());
    if (isNearFullFrame(pts, imgSize)) return false;

    return true;
}

// 判断两个已排序的四边形是不是「同一个目标」。
// 判断依据：两个中心点离得很近，就算同一个。
static bool isSameRect(const vector<cv::Point2f>& a, const vector<cv::Point2f>& b) {
    const cv::Point2f ca = centerOf(a);
    const cv::Point2f cb = centerOf(b);
    const double dist = cv::norm(ca - cb);

    const double sideA = std::min(cv::norm(a[0] - a[1]), cv::norm(a[1] - a[2]));
    const double sideB = std::min(cv::norm(b[0] - b[1]), cv::norm(b[1] - b[2]));
    const double scale = std::min(sideA, sideB);

    return dist < 0.35 * scale;
}

//使用四个点表示矩形
static vector<vector<cv::Point2f>> detectRects(const cv::Mat& bgr) {
    // 1灰度、去噪
    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);

    //  第 2 步：准备多张「黑底白块」的二值图 【我不懂】
    // 为什么要多张？因为目标可能是亮底暗块、暗底亮块、彩色、渐变底……
    // 没有哪一种二值化能通吃，所以每种都试一遍，最后把结果合起来。
    vector<cv::Mat> masks;

    // (a) Otsu 自动阈值：适合「前景背景对比明显」的图。
    //     THRESH_OTSU 会自动算出一个分界值，不用手填。
    cv::Mat otsu;
    cv::threshold(gray, otsu, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    masks.push_back(otsu);

    // (b) 同一张图反色：这样「暗目标、亮背景」的图也能被切出来。
    masks.push_back(255 - otsu);

    // (c) 自适应阈值：每个小区域单独算分界值，适合光照不均匀、渐变底的图。
    //     51 是统计范围的大小，8 是「要比周围亮出多少才算白」。
    cv::Mat adaptive;
    cv::adaptiveThreshold(gray, adaptive, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY, 51, 8);
    masks.push_back(adaptive);

    // (d) 三个颜色通道各自做一次 Otsu。
    //     有些彩色目标（比如深蓝底上的深绿块）在灰度图里几乎一样亮，
    //     但单独看某一个颜色通道就能分得很清楚。
    vector<cv::Mat> channels;
    cv::split(bgr, channels);
    for (const auto& ch : channels) {
        cv::Mat t;
        cv::threshold(ch, t, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
        masks.push_back(t);
        masks.push_back(255 - t);
    }

    // ---- 第 3、4 步：对每张二值图找轮廓，并简化成 4 个点 ----
    vector<vector<cv::Point2f>> result;
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));

    for (const auto& mask : masks) {
        // 闭运算：先膨胀再腐蚀，用来「补上」白块里的小缺口。
        // 轮廓有缺口时 approxPolyDP 会给出 5、6 个点，补一下就能回到 4 个。
        cv::Mat closed;
        cv::morphologyEx(mask, closed, cv::MORPH_CLOSE, kernel);

        // 找轮廓。RETR_LIST = 所有轮廓都要（包括白块里挖出来的洞）；
        // CHAIN_APPROX_SIMPLE = 直线段只保留端点，省内存。
        vector<vector<cv::Point>> contours;
        cv::findContours(closed, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        for (const auto& contour : contours) {
            // 周长太短的轮廓不可能是目标，提前跳过，省时间。
            const double perimeter = cv::arcLength(contour, true);
            if (perimeter < 4.0 * MIN_SIDE) continue;

            // 多边形简化：把弯弯曲曲的轮廓用尽量少的直线段表示。
            // 第二个参数是「允许误差」，越大留下的点越少。
            // 如果这是个规整的矩形，简化完就正好剩 4 个点。
            vector<cv::Point> quad;
            cv::approxPolyDP(contour, quad, APPROX_EPS_RATIO * perimeter, true);

            // ---- 第 5 步：几何条件检查 ----
            if (!looksLikeRect(quad, bgr.size())) continue;

            // 转成浮点坐标并排好顺序，后面画框、打印都用它。
            vector<cv::Point2f> pts(quad.begin(), quad.end());
            pts = orderCorners(pts);

            // ---- 第 6 步之一：去重 ----
            bool duplicated = false;
            for (const auto& kept : result) {
                if (isSameRect(pts, kept)) { duplicated = true; break; }
            }
            if (!duplicated) result.push_back(pts);
        }
    }

    // ---- 第 6 步之二：给结果排序 ----
    // 考核没有规定多目标时的输出顺序，这里按「从左到右」排，
    // 和示例里 rect1 在 rect2 左边是一致的。
    std::sort(result.begin(), result.end(),
              [](const vector<cv::Point2f>& a, const vector<cv::Point2f>& b) {
                  const cv::Point2f ca = centerOf(a);
                  const cv::Point2f cb = centerOf(b);
                  if (std::fabs(ca.x - cb.x) > 1.0f) return ca.x < cb.x;  // 先比 x
                  return ca.y < cb.y;                                     // x 接近再比 y
              });

    return result;
}

// ============================================================================
//  第 6 步：把一张图的结果打印成考核要求的格式
//
//  形如：  1.png: rect1 (120, 350); (300, 350); (300, 200); (120, 200)
//          2.png: null
//
//  注意 y 要在这里翻过来：y_输出 = (H - 1) - y_图像
// ============================================================================
static void printResult(const string& fileName, const vector<vector<cv::Point2f>>& rects, int H) {
    cout << fileName << ":";

    if (rects.empty()) {
        cout << " null" << endl;
        return;
    }

    for (size_t i = 0; i < rects.size(); ++i) {
        if (i > 0) cout << " |";          // 多个矩形之间用 " | " 分隔
        cout << " rect" << (i + 1);       // rect1、rect2 ...

        for (size_t k = 0; k < 4; ++k) {
            if (k > 0) cout << ";";       // 同一矩形内，点与点之间用 "; " 分隔

            // 坐标取最近的整数像素
            const int x = static_cast<int>(std::lround(rects[i][k].x));
            const int y = H - 1 - static_cast<int>(std::lround(rects[i][k].y));

            cout << " (" << x << ", " << y << ")";
        }
    }
    cout << endl;
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
