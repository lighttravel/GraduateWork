# PlatformIO 使用说明

## 环境准备

### 1. 安装VSCode扩展
- 安装 **PlatformIO IDE** 扩展
- VSCode -> 扩展 -> 搜索 "PlatformIO IDE" -> 安装

### 2. 验证安装
- 打开VSCode
- 左侧会出现"🐿️ PlatformIO"图标
- 点击打开PlatformIO主界面

## 项目结构

```
AIchatesp32/
├── .pio/              # PlatformIO生成文件(自动)
├── .vscode/           # VSCode配置(自动)
├── .pioignore         # 忽略文件配置
├── platformio.ini     # PlatformIO配置文件 ⭐
├── main/
│   ├── main.c         # 主程序
│   ├── config.h       # 配置文件
│   ├── drivers/       # 硬件驱动
│   ├── middleware/    # 中间件
│   └── modules/       # 应用模块
├── README.md          # 项目说明
└── TODO.md            # 开发进度
```

## 常用操作

### 📦 编译项目

**方法1: 使用VSCode界面**
1. 点击底部状态栏的 ➜️图标
2. 选择 "Build"

**方法2: 使用快捷键**
- `Ctrl+Alt+B` (Windows/Linux)
- `Cmd+Alt+B` (macOS)

**方法3: 使用命令面板**
- `Ctrl+Shift+P` -> 输入 "PIO: Build"

**方法4: 使用终端**
```bash
# 在VSCode终端中
pio run

# 或者指定环境
pio run -e esp32-s3-devkitc-1
```

### 📤 上传固件

**方法1: 使用VSCode界面**
1. 连接ESP32-S3到电脑
2. 点击底部状态栏的 ➡️图标
3. 选择 "Upload"

**方法2: 使用快捷键**
- `Ctrl+Alt+U` (Windows/Linux)
- `Cmd+Alt+U` (macOS)

**方法3: 使用终端**
```bash
# 自动检测端口
pio run --target upload

# 指定端口
pio run --target upload --upload-port COM3
```

### 🖥️ 串口监视器

**方法1: 使用VSCode界面**
1. 点击底部状态栏的 👁️图标
2. 选择 "Serial Monitor"

**方法2: 使用快捷键**
- `Ctrl+Alt+S` (Windows/Linux)
- `Cmd+Alt+S` (macOS)

**方法3: 使用终端**
```bash
# 启动监视器
pio device monitor

# 指定端口和波特率
pio device monitor -p COM3 -b 115200
```

### 🚀 一键操作

**编译 + 上传 + 监视**
```bash
# 全部自动完成
pio run --target upload && pio device monitor
```

**清理编译缓存**
```bash
pio run --target clean
```

## 配置管理

### 修改串口

**临时指定**
```bash
pio run --target upload --upload-port COM3
```

**永久修改**
编辑 `platformio.ini`:
```ini
[env:esp32-s3-devkitc-1]
upload_port = COM3  ; 修改为你的端口
```

### 查找可用串口
```bash
# Windows
pio device list

# Linux/Mac
ls /dev/tty.*
```

### 切换环境

如果配置了多个环境，可以在 `platformio.ini` 中修改:
```ini
[platformio]
default_envs = esp32-s3-wflash  ; 切换环境
```

## 高级功能

### 🐛 调试

1. 使用GDB调试器
2. 需要支持的调试器(J-Link, ESP-PROG等)

```bash
# 启动调试会话
pio debug
```

### 🔧 测试

```bash
# 运行单元测试
pio test

# 运行特定测试
pio test -e esp32-s3-devkitc-1
```

### 📊 性能分析

```bash
# 分析代码大小
pio run --target size

# 分析内存使用
pio run --target sizecomponents
```

### 📦 打包发布

```bash
# 导出编译二进制
pio run --target export

# 导出为ZIP
pio package export
```

## 常见问题

### 1. 编译错误 - "xxx.h: No such file"

**原因**: 头文件路径问题

**解决**:
- 检查 `platformio.ini` 中的 `build_flags`
- 确保所有 `.h` 文件都在正确的目录
- 添加包含路径:
```ini
build_flags =
    -I main
    -I main/drivers
    -I main/middleware
    -I main/modules
```

### 2. 上传失败 - "Failed to connect"

**原因**: 串口被占用或未连接

**解决**:
1. 检查USB连接
2. 关闭其他使用串口的程序
3. 按住ESP32的BOOT按钮，再点击RST按钮进入下载模式
4. 尝试不同的波特率

### 3. 监视器乱码

**原因**: 波特率不匹配

**解决**:
确保 `platformio.ini` 中设置的波特率与代码一致:
```ini
monitor_speed = 115200  ; 与代码中的波特率一致
```

### 4. PlatformIO无法识别设备

**解决**:
1. 安装USB驱动 (CP2102/CH340)
2. 在设备管理器中查看COM口号
3. 手动指定端口:
```bash
pio device monitor -p COM3
```

### 5. 编译太慢

**优化**:
```ini
[env]
build_flags =
    -D BUILD_OPTIMIZATION=s  ; 优化代码大小
    ; -D BUILD_OPTIMIZATION=O2  ; 优化速度

; 减少编译输出
; build_type = release  ; 不输出调试信息
```

## PlatformIO命令速查

| 命令 | 说明 |
|------|------|
| `pio run` | 编译 |
| `pio run -t upload` | 上传 |
| `pio run -t clean` | 清理 |
| `pio device monitor` | 监视器 |
| `pio device list` | 列出设备 |
| `pio test` | 运行测试 |
| `pio debug` | 调试 |
| `pio pkg install` | 安装库 |
| `pio pkg update` | 更新库 |
| `pio upgrade` | 升级PlatformIO |

## VSCode集成提示

### 底部状态栏快捷按钮

```
🔨 👁️ ➡️ 🐛 ✓ 🔔
编译 监视 上传 调试 清理 通知
```

### 侧边栏

- **🐿️ PlatformIO** - 主界面
  - 项目任务清单
  - 库管理器
  - 设备列表

### 推荐插件

除了PlatformIO IDE，还可以安装:
- **C/C++** - Microsoft官方
- **Serial Monitor** - 更好的串口工具
- **Better C++ Syntax** - 更好的语法高亮

## 编译输出位置

```
.pio/build/esp32-s3-devkitc-1/
├── firmware.elf      # ELF文件
├── firmware.bin      # 二进制文件
└── ...
```

## 更多资源

- [PlatformIO官方文档](https://docs.platformio.org/)
- [ESP32 PlatformIO指南](https://docs.platformio.org/en/latest/boards/espressif32/esp32-s3-devkitc-1.html)
- [PlatformIO论坛](https://community.platformio.org/)

---

**提示**: 遇到问题时，首先查看 `platformio.ini` 配置是否正确！
