# PlatformIO 快速开始指南

## 🚀 第一次使用？

### 步骤1: 打开项目
1. 打开VSCode
2. 文件 -> 打开文件夹
3. 选择 `AIchatesp32` 文件夹
4. 等待PlatformIO初始化（可能需要几分钟）

### 步骤2: 连接硬件
1. 使用USB线连接ESP32-S3到电脑
2. 确认设备管理器中出现COM端口
3. 如果没有，安装CH340或CP2102驱动

### 步骤3: 编译项目
点击底部状态栏的 🔨 或按 `Ctrl+Alt+B`

### 步骤4: 上传固件
1. 按住ESP32-S3的 **BOOT** 按钮
2. 点击底部状态栏的 ➡️ 或按 `Ctrl+Alt+U`
3. 等待上传完成
4. 松开BOOT按钮

### 步骤5: 查看日志
点击底部状态栏的 👁️ 或按 `Ctrl+Alt+S`

## ⚡ 常用操作速查

| 操作 | 快捷键 | 说明 |
|------|--------|------|
| 编译 | `Ctrl+Alt+B` | 编译项目 |
| 上传 | `Ctrl+Alt+U` | 烧录到设备 |
| 清理 | `Ctrl+Alt+R` | 清理编译缓存 |
| 监视器 | `Ctrl+Alt+S` | 打开串口监视器 |
| 测试 | `Ctrl+Alt+T` | 运行测试 |

## 📁 重要文件说明

```
platformio.ini          # ⭐ 项目配置(编译选项、串口等)
.pioignore             # 忽略文件配置
main/                  # 源代码目录
├── main.c             # 主程序
├── config.h           # 配置文件
├── drivers/           # 硬件驱动
├── middleware/        # 中间件
└── modules/           # 应用模块
```

## 🔧 修改配置

### 修改串口端口

打开 `platformio.ini`，找到:
```ini
upload_port = COM*  ; 改成你的端口，如 COM3
```

### 修改波特率

```ini
monitor_speed = 115200  ; 改成你需要的波特率
```

## ⚠️ 常见问题

### 编译失败？

1. 检查是否安装了PlatformIO IDE扩展
2. 点击"🐿️ PlatformIO" -> "Rebuild IntelliSense"
3. 清理缓存: `Ctrl+Alt+R`
4. 重新编译

### 上传失败？

1. 检查USB连接
2. 按住BOOT按钮再点上传
3. 尝试降低波特率:
   ```ini
   upload_speed = 460800
   ```

### 监视器乱码？

确保波特率一致:
```ini
monitor_speed = 115200  ; 必须与代码一致
```

## 📚 更多文档

- **完整使用指南**: 查看 `PLATFORMIO_GUIDE.md`
- **项目说明**: 查看 `README.md`
- **开发进度**: 查看 `todo.md`

## 💡 小技巧

1. **快速编译+上传+监视**
   - 点击底部 ➜️ 图标
   - 选择 "Upload and Monitor"

2. **自动监视**
   - 上传成功后自动打开监视器
   - 在 `platformio.ini` 中设置:
     ```ini
     upload_flags = --monitor
     ```

3. **查看详细日志**
   - `Ctrl+Shift+P` -> "PIO: Toggle Log Output"

---

**准备好了吗？开始编译你的第一个固件吧！** 🎉
