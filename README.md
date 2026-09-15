# 吉他和弦词典 · FoloToy AI Passport

一个为吉他初学者设计的和弦查看应用：在设备上快速查阅 **C 大调 / G 大调**全部 7 个调内级数的和弦，从最常用的三和弦到各类七和弦（属七 / 大七 / 小七 / 半减七）。

本目录是**独立的一套代码**，不依赖 FoloToy AI Passport 基线仓库；板级支持包（`firmware/components/bsp`，MIT 许可）复制自该基线。

## 快速开始

- **看效果**：双击打开 `index.html`（纯静态单文件，无需服务器）。左边是可交互的设备模拟器（鼠标点按键或用键盘 `↑` `↓` `Enter`），下面附全部 40 个和弦的总表。
- **跑固件**：见下文构建与烧录。设备开机直接进入和弦词典。

## 交互设计（三键）

设备只有 UP / DOWN / OK 三个键（支持单击、双击、长按）。设计原则：**练习时最常用的动作只用 UP/DOWN 翻页**，拓展功能收进 OK 键。

| 页面 | UP / DOWN | OK 单击 | OK 双击 |
| --- | --- | --- | --- |
| 选调（Chord Book，开机即此页） | C 大调 ↔ G 大调 | 进入该调 | — |
| 级数列表（I–vii°） | 移动选中行 | 查看指法 | 返回选调 |
| 和弦详情 | 上一个/下一个级数（循环翻页） | **同一根音上循环拓展变体** | 返回列表 |

页面底部有操作提示条；固件界面用英文（未内嵌中文字库），HTML 示意页下方有中文对照面板。

**拓展变体的含义**：每级默认显示调内三和弦；短按 OK 依次切换：

- 大调根音（I、IV、V 级）：三和弦 → 大七（maj7） → 属七（7）
- 小调根音（ii、iii、vi 级）：小三和弦 → 小七（m7） → 属七（7）
- vii° 级：减三和弦 → 半减七（m7♭5）

以后想加 sus4、add9、六和弦等，只需在 `chord_model.c` 对应级数的变体表**末尾追加一条**，UI 与按键语义不变——这就是预留的拓展点。

## 内容

每级 2~3 个变体，共 40 个指法，全部是开放把位常用按法：

- 小 F（xx3211）、Bm7（x2020x）等给了初学者友好的简化按法，标注 `Easy F` / `Easy grip`
- 横按和弦标注 `Barre` / `Small barre`
- F♯dim 用无低音根音的转位按法（标注 `No low root`）
- 指法图含闷音 ×、空弦 ○、按法点与指法编号（1 食指 … 4 小指）

## 目录结构

```
guitar-chords/
├── index.html            交互示意（设备模拟器 + 40 和弦总表 + 中文说明）
├── README.md             本文档
└── firmware/             独立 ESP-IDF 工程
    ├── CMakeLists.txt    工程入口（项目名 guitar-chords）
    ├── partitions.csv    分区表（保留受保护 cardid@0x356000，勿动）
    ├── sdkconfig.defaults  含 LVGL Montserrat 32 字体与 48KB LVGL 内存池
    ├── dependencies.lock 可复现的组件依赖锁定
    ├── components/bsp/   板级支持包（复制自 FoloToy AI Passport 基线, MIT）
    ├── main/             应用代码
    │   ├── main.c        初始化: 显示 + 按键, 开机直接进和弦界面
    │   ├── chords_ui.c/.h  三个页面的状态机与指法图绘制
    │   ├── chord_model.c/.h  纯 C 和弦数据(40 指法), 可主机测试
    │   └── ui_pixel.c/.h   像素风 UI 组件(取自基线)
    └── tests/            主机测试(40 指法逐一比对 + 指法不变量)
```

## 构建与烧录

需要 ESP-IDF **v5.5.3**（已安装在 `~/esp/esp-idf-v5.5.3`）：

```bash
cd firmware
source ~/esp/esp-idf-v5.5.3/export.sh
./tests/run.sh                 # 主机测试（无需硬件）
idf.py set-target esp32c3      # 仅首次
idf.py build
idf.py -p /dev/cu.usbmodem101 flash   # 分段烧录, 不会覆盖受保护的 cardid 分区
```

注意：设备已开卡时**不要**烧录合并的 8MB 全量镜像（会覆盖 cardid）；始终用分段 `idf.py flash`。

## 假设与已知边界

1. 固件界面文案为英文（设备未内嵌中文字库，引入字库超出本次范围）；
2. 横按和弦直接如实展示并标注，未做难度过滤；
3. 变体选择只在会话内记忆（重启后复位），未做 NVS 持久化；
4. 真机已验证：启动日志、开机直进和弦词典、页面切换不再重启；指法图显示效果与按键手感请上手确认。
