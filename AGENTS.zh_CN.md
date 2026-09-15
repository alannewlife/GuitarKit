<p align="right">
  <strong>简体中文</strong> · <a href="AGENTS.md">English</a>
</p>

# AI 助手仓库须知（AGENTS.md）

独立的**吉他工具箱 Guitar Kit**，运行在 FoloToy AI Passport 上（ESP32-C3，8MB Flash，无 PSRAM，ESP-IDF 5.5.3）。内含四件工具：和弦词典、调音器、变调夹速查、节拍器。产品介绍见 [README.md](README.md)；本文件写的是 AI 助手**不能搞错**的规则。

## 固件产物：两种类型、两种刷法

这是最容易出事故的地方。仓库里有**两种不同的 `.bin`**，**不能互换用途**：

| 产物 | 内容 | 写入位置 | 用途 |
| --- | --- | --- | --- |
| `build/guitar-kit.bin`（及日期副本 `guitar-kit-YYYYMMDD.bin`） | 仅应用 —— 无引导、无分区表 | `0x10000`，需配合引导和分区表一起分段写入 | **只用于自己升级**，走分段 `idf.py flash` |
| `build/guitar-kit-full-YYYYMMDD.bin`（由 `tools/merge.sh` 生成） | 合并镜像：引导 + 分区表 + 应用 | `0x0`，整卡一次写入 | **提交商店审核**、空白设备 |

### 事故记录（不要重演）

一次商店审核提交用了**纯应用** bin，被驳回："固件无法正常启动，请检查是否能从 0x0 分区刷写的合并固件"。审核流水线会把收到的固件**整体写到 0x0**——纯应用镜像刷在那里没有引导和分区表，无法启动。

**规则：**

1. 商店审核 / 任何从 0x0 整写的流水线 → **必须提交合并镜像**（`tools/merge.sh` 产出的 `guitar-kit-full-YYYYMMDD.bin`），绝不能提交 `guitar-kit.bin`。
2. 用户手上的设备是**已开卡设备**（`0x356000` 处有受保护的 `cardid` 分区，存出厂身份数据）。自己升级必须走**分段 `idf.py flash`** —— 绝不能把合并镜像刷进它。
3. 改名或移动本目录后，先 `idf.py fullclean` 再 build —— 缓存的构建目录记着旧绝对路径，会报错或刷出旧固件。

## 构建、测试、烧录、提审

```bash
cd firmware
source ~/esp/esp-idf-v5.5.3/export.sh
./tests/run.sh                  # 主机测试（和弦/调音/变调/节拍器）—— 不需要硬件
idf.py set-target esp32c3       # 仅首次（按 defaults 重新生成 sdkconfig）
idf.py build                    # 产出应用 bin + 日期副本 guitar-kit-YYYYMMDD.bin
idf.py -p /dev/cu.usbmodemXXX flash          # 自己升级（分段）
tools/merge.sh                  # 商店提审（合并全量镜像，自带布局校验）
```

修改过 `sdkconfig.defaults` 后也需要重新 `idf.py set-target esp32c3`（增量构建不会拾取新的 Kconfig 选项）。

## 代码组织规则

- 与硬件无关的纯逻辑（和弦、测频、变调换算、节拍速度）各自独立成模块，并配 `firmware/tests/run.sh` 里的主机测试 —— 不允许 include ESP-IDF 或 LVGL。
- 新工具加入 `main/chords_ui.c` 的 `HUB_ITEMS` 表，首页网格自动生长；再接上对应页面分支、按键处理，以及（有动画时）100ms 的 `tuner_tick`。
- 页面整屏重建时控件指针会先清零 —— 页面构建函数重建它们之前绝不能访问（曾因向 NULL 标签写文本导致设备重启）。
- LVGL 内存池 48KB；页面重建必须**先删旧屏再建新屏**，两屏不能共存。
- 本机 USB 连接不稳：动过硬件或目录后，先确认 `/dev/cu.usbmodem*` 存在再操作；USB-JTAG 卡死时重新插拔即可恢复。

更多背景见 [README.md](README.md)（产品、手势、构建细节）。
