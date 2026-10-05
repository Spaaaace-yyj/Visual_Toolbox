# 视觉工具箱 · Visual Toolbox 0.2.0

## 本项目纯AI 零人工

C++17 + Qt Widgets + OpenCV 的 Ubuntu 桌面工具。当前模块是标定板，支持**屏幕毫米显示**与**自定义纸张 PDF 打印**，共用棋盘格、对称/非对称圆点阵、ChArUco 和 AprilGrid 参数。

## 安装与编译

Ubuntu 22.04 / 24.04：

```bash
sudo apt update
sudo apt install build-essential cmake qtbase5-dev libopencv-dev libopencv-contrib-dev fonts-noto-cjk

cd visual-toolbox
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$(nproc)"
./build/visual-toolbox
```

CLion 可直接打开包含 `CMakeLists.txt` 的目录。程序不依赖 ROS 或 Python，PDF 导出使用 Qt GUI，自定义页面尺寸与标定图案全部由 C++ 完成。

已在 Qt 5.15 + OpenCV 4.6 编译、测试；图案测试也已使用 Ubuntu 22.04 的 OpenCV 4.5.4 动态库通过。新版 OpenCV 4.7+ 接口有相应适配，该分支未在本交付中实测。Qt 6 编译分支亦未实测。

如果 CMake 优先选中了缺少 ArUco 的自编译 OpenCV，可使用系统版本：

```bash
cmake -S . -B build-system -DOpenCV_DIR=/usr/lib/x86_64-linux-gnu/cmake/opencv4
cmake --build build-system -j "$(nproc)"
./build-system/visual-toolbox
```

## 屏幕显示

1. 设置目标板类型、行列数、单元毫米尺寸、外围白边。
2. 选择“屏幕显示”页与目标屏幕。默认尝试检测尺寸；已保存的直尺校准结果优先使用。
3. 自动检测优先从匹配的 Linux DRM 显示接口读取 EDID；不可用时尝试 Qt 的系统屏幕物理尺寸。界面显示尺寸来源与“尚未实测”。
4. 点击全屏显示，先用尺子验证多个格子的总长度与高度。若比例不准，点击“用直尺校准屏幕”，分别测量两条参考线的端点刻线中心距离，按 Esc 后输入毫米长度。
5. 校准结果保存后直接用于该屏幕/显示模式。全屏时按 Esc 返回；双屏可在控制屏修改参数，目标屏实时更新。

“尝试自动检测 / 使用自动尺寸”会切换回自动估计，保留原有直尺测量数据。直尺重新校准后自动优先使用新的实测值。旧版屏幕校准配置仍保存在同一 Qt QSettings 路径，无需重新输入。

### 自动检测的限度

EDID 是显示器声明的参数，系统的物理尺寸也可能是估算值。软件不能自动判断声明值是否等于真实面板尺寸，因此明确标记“尚未实测”。读取失败、尺寸明显不合理或显示模式宽高比不匹配时，不使用该自动结果，需要手动校准。

优先使用原生分辨率。若使用分数缩放，必须在当前模式下复测。屏幕名称、设备信息、分辨率与缩放共同识别配置；无序列号的同名屏幕仍可能匹配旧配置，更换实物屏幕后需重新测量。

计算采用 Qt **逻辑尺寸 / 物理毫米尺寸**，再由 Qt 处理缩放，避免重复乘设备像素比例。手动校准分别保存横纵比例：`参考线坐标长度 / 实测毫米长度`。

## PDF 打印

1. 选择“PDF 打印”页。上方棋盘参数继续有效。
2. 选择 A4、A3，或直接输入自定义纸张宽、高，单位 mm；支持小数。预设默认为横向，按钮可交换宽高。
3. 设置打印页边距。目标板的“外围白边”和纸张的“页边距”是两个独立参数。
4. 右侧预览纸张上的居中布局。虚线只显示可用区域，不输出到 PDF。
5. 点击“导出实际尺寸 PDF”。程序按设置的格子/tag 尺寸导出，**不会自动缩小来适应纸张**。目标板含白边超出可用区域时禁用导出并给出尺寸提示。
6. 打印时选择“实际大小 / 100%”，关闭“适合页面 / 自动缩放”。如果打印机不支持设定纸张或边距，需要调整打印机设置与纸张参数。打印后用尺子复测格边或 tag 边长。

PDF 导出与屏幕尺寸校准独立，不需要连接或测量目标显示器。棋盘、tag 编码模块、辅助黑块和圆点均使用矢量图形，放大或打印不会发生位图插值。程序修正 Qt 默认整数 PDF 点造成的纸张尺寸舍入，并保留图案毫米尺度与居中位置。

提供的示例位于 `examples/`：

| 文件 | 纸张 | 目标板 |
|---|---|---|
| chessboard-A4.pdf | 297 × 210 mm | 9 × 7 方格，格边 20 mm，白边 20 mm |
| charuco-custom.pdf | 320.50 × 240.25 mm | 8 × 6 方格，格边 20 mm，marker 14 mm，白边 15 mm |
| aprilgrid-A3.pdf | 297 × 420 mm | 6 × 8 tag，tag 边 24 mm，间隙比例 0.3，白边 15 mm |

这些示例展示导出能力；你可以在软件中重新设置任意支持的参数。屏幕或打印实物精度仍受像素间距、量尺误差、打印机缩放和纸张变形影响。

## 参数约定

| 图案 | 行列数含义 | 单元尺寸含义 | Kalibr 配置 |
|---|---|---|---|
| 棋盘格 | 实际方格数 | 方格边长 mm | checkerboard，内角点数自动减一 |
| 对称圆点阵 | 圆点数 | 相邻圆心距离 mm | circlegrid |
| 非对称圆点阵 | 每行点数、行数 | 基本间距 mm | circlegrid |
| ChArUco | 方格数 | 格边和独立 marker 边长 mm | Kalibr 不直接支持 |
| AprilGrid | tag 数 | tag 黑色外边界边长 mm | aprilgrid |

非对称圆点坐标：`x=(2*列号 + 行号%2)*基本间距`，`y=行号*基本间距`；同一行相邻圆心距离为两倍基本间距。

AprilGrid 使用 `tag36h11`，两层黑色边框和 Kalibr 的标签朝向。ID 0 在左下角，向右、再向上逐行编号。`tagSpacing = 间隙 / tag 边长`，内部与外围交点的辅助实心黑块边长等于间隙；外围辅助黑块在既有白边内绘制，白边不足时裁剪，不改总尺寸。OpenCV 检测时应设 `markerBorderBits=2`。首次接入所用 Kalibr 版本，应检查标签/角点对应关系。

ChArUco 使用 `DICT_4X4_1000`。程序按实际标签角点方向转换旧版向上 Y 坐标，保留当前 OpenCV 生成器的 ID、朝向与黑白格约定。检测端需要使用相同 OpenCV 版本或兼容的 legacy pattern 约定。

## 配置和预设

- JSON v2 保存目标板参数和纸张参数；旧版 JSON v1 仍能加载。
- 屏幕校准数据独立保存于 `~/.config/Lichuang/ScreenCalibrator.conf`，保持与旧版兼容。
- 纸张参数在成功导出后记忆，下次启动恢复。
- 自动估计不会假装成直尺校准，也不把屏幕比例写入可迁移的目标板预设。

## 验证

```bash
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/visual-toolbox --smoke /tmp/visual-toolbox-qa
QT_QPA_PLATFORM=offscreen ./build/toolbox-tests /tmp/visual-toolbox-pdfs
```

测试覆盖全部图案检测、ChArUco 奇偶行列/完整角点、AprilGrid 辅助方块、EDID 校验与毫米尺寸、横竖显示及缩放、纸张越界和 PDF 导出。

可选 PDF 文件独立验证，需要 `pypdf` 和 `pdfplumber`：

```bash
python tests/verify_pdfs.py /tmp/visual-toolbox-pdfs
```

它独立读取 PDF 页面宽高与矢量几何，检查 20 mm 格边、居中位置、自定义小数纸张以及无位图内容。交付示例也已用 Poppler 渲染检查；离屏测试不能替代你的真实显示器或打印机复测。

## 源码结构

```text
src/board.*           标定板毫米几何、矢量绘制与 Kalibr 配置
src/screen_metrics.*  EDID 读取、自动尺寸和逻辑坐标换算
src/pdf_export.*      纸张校验、排版、PDF 写入及精确页面尺寸
src/window.*          参数界面、屏幕/打印分页、校准与预设
src/main.cpp          桌面应用入口
```

参考：Qt QScreen https://doc.qt.io/qt-5/qscreen.html ；QPdfWriter https://doc.qt.io/qt-5/qpdfwriter.html ；Kalibr https://github.com/ethz-asl/kalibr/wiki/calibration-targets 。

power by-ChatGPT6

