## Thumbnailer

为视频文件生成缩略图，基于 FFmpeg 6.0，使用 Qt6 制作界面，通过 qmake 构建

##### 基本用法

可以查看视频文件，并在指定的位置截取画面，最终合并为一个 n\*m 的大缩略图，或者也可以将这 n\*m 张图像单独生成。

##### 调用方法

可以将这个项目作为 Qt 工程的一个子项目（.pri）嵌入，但它有时会崩溃连累到主项目，因此更推荐将其独立出来编译为可执行文件，通过进程间通信的方式来实现原有功能。

##### 项目结构

`ThumbListener` 类用于在后台新开一个线程监听有没有输入，并作出响应。

`player/` 文件夹下的类通过实现 `VideoPlayer` 中的接口实现视频播放功能。

`api/ThumbsGetter.hpp` 是接收方负责调用本程序功能的实现代码，它会放在另一个项目中运行。

`3rdparty/` 文件夹下存放用到的第三方工具:
    - ffmpeg: 官方的 ffmpeg 6.0 源码
    - GIFWriter: 生成 gif 图片

##### 通信协议

ThumbListener 通过标准输入输出进行进程间通信，每行一条 JSON 消息。所有命令通过 `opt` 字段区分，不区分大小写。运行时数据存放在可执行文件同级目录的 `ztbso/` 下（`ztbso/merged/` 存放合并缩略图，`ztbso/single/` 存放临时单张缩略图，程序退出时自动清理）。

##### 支持的指令

| 指令 | 说明 | 输入字段 | 输出字段 |
|------|------|----------|----------|
| `get_media_info` | 获取视频文件信息 | `file_path` | `width`, `height`, `duration`, `result` |
| `get_thumbnails` | 生成单张缩略图（保存为临时文件） | `file_path`, `count`(1~81), `pts_list`(可选, ms, 逗号分隔) | `pos`, `thumb_path`, `progress`, `result` |
| `get_merged_thumbnails` | 生成合并的 n×m 缩略图 | `file_path`, `row`(1~9), `column`(1~9), `pts_list`(可选), `thumbs_name`(可选) | `thumbs_path`, `progress`, `result` |
| `set_window_opacity` | 设置窗口透明度 | `opacity`(0~1) | `result` |
| `delete_file` | 删除 ztbso 目录内的文件(一般不需要手动调用) | `file_path` | 无 |
| `exit` | 停止监听并退出程序 | 无 | 无 |

对于所有指令，如果其有输出内容，那么必定会包含 `result` 字段，"Success" 表示正常执行，其他则表示失败信息。

**示例：**

```json
{"opt": "get_thumbnails", "file_path": "D:/video.mp4", "count": 9, "pts_list": "1000,5000,10000"}
```

```json
{"opt": "get_merged_thumbnails", "file_path": "D:/video.mp4", "row": 3, "column": 3}
```

**说明：**
- `get_thumbnails` 和 `get_merged_thumbnails` 为异步操作，执行过程中会持续输出 `progress`（0~100）消息
- `get_thumbnails` 完成后逐张返回结果，每条包含 `pos`（从1开始）和 `thumb_path`
- `get_merged_thumbnails` 完成后返回 `thumbs_path`（保存在 `ztbso/merged/` 下）
- `delete_file` 仅允许删除 `ztbso/` 目录内的文件，不会操作外部路径
- 所有响应均包含 `opt` 和 `file_path` 字段（如适用），通过 `result` 字段判断是否成功

