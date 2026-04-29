# GIFWriter 文档

## 项目简要描述

Qt C++ 项目中的 GIF 动图生成子模块。底层使用开源库 **cgif**（C 语言，含 LZW 压缩、颜色量化、帧间差分优化），上层由 `GifEncoder` 类封装为 Qt 友好的接口。

基本用法：`open()` → 循环 `push()` → `close()`。
可选压缩：在 `open()` 前调用 `setQuality(1~10)` 控制文件体积，默认 10（最高质量），值越小体积越小。不调用则与原始行为完全一致。

## 核心文件

| 文件 | 作用 |
|------|------|
| `gifencoder.h/cpp` | **我们维护的 Qt 封装层**。提供 `open()/push()/close()` 三步式 API |
| `cgif_rgb.c/cpp` | RGB 图像→索引色转换、颜色量化（mean-cut 决策树）、Floyd-Steinberg 抖动 |
| `cgif.c/cpp` | 帧队列管理、帧间透明优化、差分窗口优化 |
| `cgif_raw.c/cpp` | 底层 GIF 字节流写入：LZW 编码、GIF89a 头、NETSCAPE 循环扩展 |
| `cgif.h` / `cgif_raw.h` | 所有公共类型定义和 API 声明 |

`.c` 为原始 C 源码，`.cpp` 为 C++ 编译适配版（加了强制类型转换，去掉了 goto 跨声明等）。两份逻辑需同步维护。

## 本轮已完成的修复

**gifencoder.cpp/h（6 项）：**
1. `open()` 悬空指针：`toLocal8Bit()` 临时对象销毁后 `config.path` 悬空 → 用 `QByteArray` 局部变量持有
2. 图片格式：未确保 RGBA8888 → 加 `.convertToFormat(QImage::Format_RGBA8888)`
3. 图片缩放：注释说缩放但没做 → 加 `.scaled(gifWidth, gifHeight)`
4. **delay 单位错误（播放速度不对的根因）**：用户传毫秒，GIF 标准是厘秒(10ms) → `delayTime / 10`
5. `close()` 双重释放：`cgif_rgb_close` 内部无论成败都 free → 现在始终置 `pGIF = nullptr`
6. 重复 `open()` 泄漏：开头检测并关闭旧 GIF

**cgif_rgb.c/cpp（1 项，开源库 bug）：**
7. `rgb_to_index` 无量化路径：`memcmp(&pImageData[...])` 读了未初始化的输出缓冲区 → 改为 `pImageDataRGB`

## 压缩质量功能（setQuality）

新增 `GifEncoder::setQuality(int quality)` 方法，在 `open()` 前调用，范围 1~10，默认 10（与原始行为完全一致）。不调用则对上层无感。

**实现原理——三个压缩手段联动：**

| quality | depthMax | 最大色数 | 抖动 | 帧跳过阈值 |
|---------|----------|---------|------|-----------|
| 10~9    | 8        | 255     | Floyd-Steinberg | 无 |
| 8~7     | 7        | 127     | Floyd-Steinberg | 无 |
| 6~5     | 6        | 63      | 无   | 0.5% |
| 4~3     | 5        | 31      | 无   | 1.0% |
| 2~1     | 4        | 15      | 无   | 2.0% |

1. **减少色数**（`depthMax` 下调）：色数减少让 LZW 符号空间更小、重复模式更多，压缩率大幅提升。改动位于 `cgif.h`（`CGIFrgb_FrameConfig` 新增 `depthMax`/`dithering` 字段）和 `cgif_rgb.c/cpp`（`cgif_rgb_addframe` 读取新字段替代硬编码）。
2. **关闭抖动**：抖动引入的噪声会破坏像素间重复模式，关闭后 LZW 压缩更高效。屏幕录制场景（大面积纯色块）画质损失不明显。
3. **帧跳过**：在 `gifencoder.cpp` 的 `push()` 中逐像素比较当前帧与上一帧，差异率低于阈值则累计延时并跳过写入。比较中有提前退出优化。

**向后兼容设计：**
- `CGIFrgb_FrameConfig` 新字段 `depthMax`/`dithering` 在 `memset(0)` 后自动取默认值（`depthMax=0→8`，`dithering=0→Floyd-Steinberg`），与原始硬编码行为一致。
- `open()/push()/close()` 签名不变。

## 已知残留问题

- `cgif_rgb.cpp:540` 有 `#include<QDebug>`，clangd 可能报找不到头文件，实际编译取决于 Qt 构建系统配置，非本次引入。
- cgif 库内部大量 `malloc` 无返回值检查（标注 `// TBD`），属上游技术债，暂不修。
- `crawl_decision_tree` 中 `nodeList[512]` 固定大小数组，理论上 colMax=255 时刚好够用（最大索引 508），但没有越界保护。

## 开发约束

- GIF delay 最小精度为 10ms（1 厘秒），且多数浏览器/播放器会将 <20ms 的值钳制为 100ms。
- `.c` 和 `.cpp` 文件必须同步修改，否则链接时会出现符号冲突或行为不一致。
- `QImage` 传入 `push()` 前无需手动转格式或缩放，封装层已处理。
- `setQuality()` 应在 `open()` 前调用；`quality=10` 为默认值，等价于不调用。
- `depthMax` 降低后 `nodeList[512]` 仍足够（`depthMax=4` 时 `colMax=15`，远小于 255）。
