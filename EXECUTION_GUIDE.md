# 快速执行手册 - 补充文档

> 此文档补充 PROJECT_HANDOFF.md，提供更详细的操作步骤和示例。

---

## 🚀 立即开始测试后端

### 1. 环境准备（5分钟）

```bash
# 1.1 检查 Python 版本
python --version  # 需要 3.11+

# 1.2 检查 PostgreSQL
psql --version    # 需要 15+

# 1.3 创建数据库
psql -U postgres
CREATE DATABASE aromatherapy_db;
\q

# 1.4 进入后端目录
cd E:\graduatework\test1\backend

# 1.5 创建虚拟环境
python -m venv venv
venv\Scripts\activate  # Windows
# source venv/bin/activate  # Linux/Mac

# 1.6 安装依赖
pip install -r requirements.txt

# 1.7 初始化数据库表
psql -U postgres -d aromatherapy_db -f migrations\001_initial_schema.sql

# 1.8 验证 .env 文件存在
# 文件已存在，无需修改（API密钥已配置）
```

### 2. 启动后端服务器

```bash
# 方式 1: 直接运行
python main.py

# 方式 2: 使用 uvicorn（推荐，支持热重载）
uvicorn main:app --reload --host 0.0.0.0 --port 8000

# 看到以下输出表示成功：
# INFO:     Uvicorn running on http://0.0.0.0:8000
# INFO:     Application startup complete.
```

### 3. 验证后端运行

打开浏览器访问：
- **API 文档**: http://localhost:8000/docs（Swagger UI）
- **健康检查**: http://localhost:8000/health

预期响应（/health）：
```json
{
  "status": "healthy",
  "environment": "development",
  "services": {
    "database": "connected",
    "iflytek_asr": "configured",
    "iflytek_tts": "configured",
    "llm": "configured"
  }
}
```

---

## 🧪 测试 API 端点（使用 curl 或 Postman）

### 测试 1: TTS 语音合成

```bash
# 生成语音（返回 MP3 音频）
curl -X POST "http://localhost:8000/api/tts" \
  -H "Content-Type: application/json" \
  -d '{
    "text": "设置柠檬香型，强度为7，持续30分钟",
    "voice": "xiaoyan",
    "speed": 50,
    "volume": 50
  }' \
  --output test_audio.mp3

# 播放音频验证
# Windows: start test_audio.mp3
# Linux: mpg123 test_audio.mp3
```

### 测试 2: 获取可用语音列表

```bash
curl -X GET "http://localhost:8000/api/tts/voices"
```

预期响应：
```json
{
  "voices": [
    {
      "name": "xiaoyan",
      "language": "zh_CN",
      "gender": "female",
      "description": "Standard female voice (Mandarin)"
    },
    ...
  ],
  "default": "xiaoyan"
}
```

### 测试 3: WebSocket 命令执行（使用 websocat 或浏览器控制台）

#### 方式 1: 使用浏览器控制台

打开浏览器控制台（F12），粘贴以下代码：

```javascript
// 连接 WebSocket
const ws = new WebSocket('ws://localhost:8000/api/ws/commands');

ws.onopen = () => {
  console.log('✅ WebSocket 连接成功');

  // 发送命令执行请求
  ws.send(JSON.stringify({
    type: 'execute_command',
    user_input: '把房间调成清新的感觉'
  }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('📨 收到事件:', data.type, data.data);
};

ws.onerror = (error) => {
  console.error('❌ WebSocket 错误:', error);
};
```

预期输出（按顺序）：
```
✅ WebSocket 连接成功
📨 收到事件: device_status {...}
📨 收到事件: llm_processing {...}
📨 收到事件: command_generated {...}
📨 收到事件: command_saved {...}
📨 收到事件: device_executing {...}
📨 收到事件: device_executed {...}
📨 收到事件: tts_generating {...}
📨 收到事件: tts_ready {...}
📨 收到事件: execution_complete {...}
```

#### 方式 2: 使用 Python 测试脚本

创建 `test_websocket.py`：

```python
import asyncio
import websockets
import json

async def test_command():
    uri = "ws://localhost:8000/api/ws/commands"

    async with websockets.connect(uri) as websocket:
        print("✅ WebSocket 连接成功")

        # 发送命令
        await websocket.send(json.dumps({
            "type": "execute_command",
            "user_input": "设置薰衣草香味，强度5，持续60分钟"
        }))

        # 接收事件
        for i in range(10):
            try:
                message = await asyncio.wait_for(websocket.recv(), timeout=5.0)
                event = json.loads(message)
                print(f"📨 事件 {i+1}: {event['type']}")
                if event['type'] == 'execution_complete':
                    break
            except asyncio.TimeoutError:
                print("⏱️ 超时，停止接收")
                break

asyncio.run(test_command())
```

运行：
```bash
python test_websocket.py
```

---

## 🎯 给 Codex 的详细提示词示例

### 示例 1: 创建 App.tsx

```
我正在开发一个 AI 语音控制香薰系统的前端。

当前状态:
- ✅ 项目配置完成（Vite + React + TypeScript）
- ✅ 类型定义: src/types/aromatherapy.ts
- ✅ API 客户端: src/services/apiClient.ts
- ✅ 状态管理: src/store/commandStore.ts（Zustand）
- ⏳ 需要创建: src/App.tsx（主应用组件）

任务: 创建 src/App.tsx

要求:
1. 使用 React Router v6 配置路由
2. 路由配置:
   - / → 重定向到 /dashboard
   - /dashboard → Dashboard 页面（稍后创建）
3. 布局结构:
   - 顶部导航栏（应用标题 + 连接状态指示）
   - 主内容区域（<Outlet />）
4. 全局样式: 深色主题，主色调为蓝色
5. 错误边界: 包裹整个应用

代码规范:
- TypeScript 严格模式
- 函数组件 + Hooks
- 导入路径使用 @ 别名（@/components/...）

请提供完整代码。
```

### 示例 2: 创建 useAudioCapture Hook

```
我正在开发一个 AI 语音控制香薰系统的前端。

当前状态:
- ✅ 前端基础设施已搭建
- ⏳ 需要创建: src/hooks/useAudioCapture.ts

任务: 创建 useAudioCapture.ts

功能需求:
1. 使用 MediaRecorder API 捕获麦克风音频
2. 音频格式要求:
   - 采样率: 16kHz
   - 位深: 16bit
   - 声道: 单声道（mono）
   - 编码: PCM（raw）
3. 返回接口:
   ```typescript
   {
     startRecording: () => Promise<void>
     stopRecording: () => void
     audioChunks: Blob[]
     isRecording: boolean
     error: string | null
     hasPermission: boolean
   }
   ```
4. 错误处理:
   - 捕获权限拒绝（NotAllowedError）
   - 捕获设备不可用（NotFoundError）
   - 设置友好错误消息

技术要求:
- React Hook
- TypeScript 类型完整
- 清理函数（useEffect cleanup）
- 注释详细

请提供完整代码。
```

### 示例 3: 创建 VoiceCapture 组件

```
我正在开发一个 AI 语音控制香薰系统的前端。

当前状态:
- ✅ useAudioCapture Hook 已创建
- ✅ useASRWebSocket Hook 已创建
- ⏳ 需要创建: src/components/VoiceCapture.tsx

任务: 创建 VoiceCapture.tsx

UI 需求:
1. 按住说话按钮:
   - 设计: 圆形按钮，中央麦克风图标
   - 交互: onMouseDown 开始录音，onMouseUp 停止录音
   - 录音时: 红色边框动画（呼吸效果）
2. 转录文本显示:
   - 部分转录: 灰色文字
   - 最终转录: 白色文字，加粗
3. 状态指示:
   - 未录音: "按住说话"
   - 录音中: "正在录音..." + 红点动画
   - 转录中: "识别中..."
4. 错误提示:
   - 权限拒绝: "请允许麦克风权限"
   - 连接失败: "语音服务连接失败"

技术要求:
- 使用 useAudioCapture 和 useASRWebSocket Hooks
- CSS 使用内联样式或 styled-components
- 响应式设计（最小宽度 300px）
- 无障碍: aria-label, role 属性

请提供完整代码（包含样式）。
```

---

## 🐛 常见错误及解决方案

### 错误 1: ModuleNotFoundError: No module named 'anthropic'

**原因**: 虚拟环境未激活或依赖未安装

**解决**:
```bash
# 激活虚拟环境
venv\Scripts\activate  # Windows

# 重新安装依赖
pip install -r requirements.txt
```

### 错误 2: sqlalchemy.exc.OperationalError: could not connect to server

**原因**: PostgreSQL 未启动或连接字符串错误

**解决**:
```bash
# 1. 检查 PostgreSQL 服务
# Windows: 服务管理器中启动 postgresql-x64-15
# Linux: sudo systemctl start postgresql

# 2. 验证 .env 中的 DATABASE_URL
# 格式: postgresql+asyncpg://user:password@localhost:5432/aromatherapy_db

# 3. 测试连接
psql -U postgres -d aromatherapy_db -c "SELECT 1;"
```

### 错误 3: WebSocket connection failed (ERR_CONNECTION_REFUSED)

**原因**: 后端未启动或端口被占用

**解决**:
```bash
# 1. 检查后端是否运行
curl http://localhost:8000/health

# 2. 检查端口占用
# Windows: netstat -ano | findstr :8000
# Linux: lsof -i :8000

# 3. 更改端口（如果 8000 被占用）
# 修改 .env: APP_PORT=8001
# 修改 frontend/vite.config.ts 代理配置
```

### 错误 4: CORS policy: No 'Access-Control-Allow-Origin' header

**原因**: 前端地址未加入后端 CORS 白名单

**解决**:
```bash
# 修改 backend/.env
CORS_ORIGINS=http://localhost:5173,http://localhost:3000

# 重启后端服务
```

### 错误 5: LLM API 返回 401 Unauthorized

**原因**: API Token 错误或过期

**解决**:
```bash
# 验证 .env 中的凭证
ANTHROPIC_AUTH_TOKEN=daf6f0cdfc6a49f79fcc3496bb379605.1W23CEHM12iYFaU4
ANTHROPIC_BASE_URL=https://open.bigmodel.cn/api/anthropic

# 测试 API 连接
curl -X POST "https://open.bigmodel.cn/api/anthropic/v1/messages" \
  -H "Authorization: Bearer daf6f0cdfc6a49f79fcc3496bb379605.1W23CEHM12iYFaU4" \
  -H "Content-Type: application/json" \
  -d '{"model":"glm-4","messages":[{"role":"user","content":"测试"}],"max_tokens":100}'
```

---

## 📋 分步骤任务清单（复制给 Codex）

### 任务清单：Phase 6 剩余部分

**[ ] 任务 6.1: 创建全局样式**
```
文件: frontend/src/index.css
内容:
- CSS Reset（*, *::before, *::after）
- 深色主题配色方案
- 全局字体（system-ui 或 Inter）
- 滚动条样式
- 基础布局类（.container, .flex-center 等）
```

**[ ] 任务 6.2: 创建 App.tsx**
```
文件: frontend/src/App.tsx
功能:
- React Router 配置（/ → /dashboard）
- 顶部导航栏
- ErrorBoundary 包裹
- 布局结构（header + main）
```

**[ ] 任务 6.3: 创建 Dashboard 页面骨架**
```
文件: frontend/src/pages/Dashboard.tsx
功能:
- 3 列布局（语音控制 | 可视化 | 命令面板）
- 占位符组件（稍后替换为真实组件）
- 响应式设计（<768px 单列）
```

### 任务清单：Phase 7 语音采集

**[ ] 任务 7.1: useAudioCapture Hook**
```
文件: frontend/src/hooks/useAudioCapture.ts
核心逻辑:
- navigator.mediaDevices.getUserMedia({ audio: true })
- MediaRecorder 配置（mimeType: 'audio/webm'）
- 音频块收集（ondataavailable）
- 权限错误处理
```

**[ ] 任务 7.2: useASRWebSocket Hook**
```
文件: frontend/src/hooks/useASRWebSocket.ts
核心逻辑:
- WebSocketManager 实例化（endpoint: '/ws/asr'）
- 发送二进制音频流
- 解析 ASR 事件（partial/final）
- 自动重连
```

**[ ] 任务 7.3: VoiceCapture 组件**
```
文件: frontend/src/components/VoiceCapture.tsx
UI 元素:
- 圆形按钮（直径 80px）
- 麦克风图标（使用 SVG 或 Unicode 🎤）
- 录音动画（CSS keyframes）
- 转录文本框（实时更新）
```

### 任务清单：Phase 8 可视化

**[ ] 任务 8.1: useCommandWebSocket Hook**
```
文件: frontend/src/hooks/useCommandWebSocket.ts
核心逻辑:
- WebSocketManager 实例化（endpoint: '/ws/commands'）
- 监听 11 种事件类型
- 自动更新 Zustand store（useCommandStore）
- 发送命令（execute_command, get_status, stop_device）
```

**[ ] 任务 8.2: DeviceStatus 组件**
```
文件: frontend/src/components/DeviceStatus.tsx
显示字段:
- is_active（激活状态 - 绿/灰指示灯）
- current_scent（香型图标 + 名称）
- current_intensity（0-10 进度条）
- remaining_minutes（倒计时 MM:SS）
```

**[ ] 任务 8.3: AromatherapyCharts 组件**
```
文件: frontend/src/components/AromatherapyCharts.tsx
图表:
1. 强度柱状图（Recharts BarChart）
2. 混合比例饼图（Recharts PieChart）
3. 持续时间进度条（自定义 SVG）
4. 释放节奏动画（CSS animations）
```

**[ ] 任务 8.4: CommandPanel 组件**
```
文件: frontend/src/components/CommandPanel.tsx
区域:
- 用户输入显示
- LLM 响应文本
- JSON 控制指令（代码高亮）
- 执行步骤进度条
```

---

## 🎨 UI/UX 设计建议

### 配色方案（深色主题）

```css
:root {
  /* 主色调 */
  --primary: #3B82F6;      /* 蓝色 */
  --primary-dark: #2563EB;
  --primary-light: #60A5FA;

  /* 香型颜色 */
  --scent-lemon: #FCD34D;   /* 柠檬-黄 */
  --scent-lavender: #C4B5FD; /* 薰衣草-紫 */
  --scent-woody: #A78BFA;    /* 木质-棕 */
  --scent-floral: #F9A8D4;   /* 花香-粉 */

  /* 背景色 */
  --bg-primary: #0F172A;     /* 深蓝黑 */
  --bg-secondary: #1E293B;   /* 次要背景 */
  --bg-tertiary: #334155;    /* 卡片背景 */

  /* 文字颜色 */
  --text-primary: #F1F5F9;   /* 主文字 */
  --text-secondary: #94A3B8; /* 次要文字 */
  --text-muted: #64748B;     /* 灰色文字 */

  /* 状态颜色 */
  --success: #10B981;        /* 绿色-成功 */
  --warning: #F59E0B;        /* 黄色-警告 */
  --error: #EF4444;          /* 红色-错误 */
}
```

### 组件尺寸规范

```
按钮:
- 小: 32px height, 12px padding
- 中: 40px height, 16px padding
- 大: 48px height, 20px padding

间距:
- xs: 4px
- sm: 8px
- md: 16px
- lg: 24px
- xl: 32px

圆角:
- sm: 4px
- md: 8px
- lg: 12px
- full: 9999px（圆形）

阴影:
- sm: 0 1px 2px rgba(0,0,0,0.1)
- md: 0 4px 6px rgba(0,0,0,0.2)
- lg: 0 10px 15px rgba(0,0,0,0.3)
```

---

## 📦 部署建议（可选）

### 后端部署（Railway / Render）

```bash
# 1. 创建 Procfile
echo "web: uvicorn main:app --host 0.0.0.0 --port $PORT" > Procfile

# 2. 创建 runtime.txt
echo "python-3.11.7" > runtime.txt

# 3. 修改数据库 URL（使用环境变量）
# Railway/Render 会自动提供 DATABASE_URL

# 4. 设置环境变量（在平台控制台）
# 复制 .env 内容到平台的 Environment Variables
```

### 前端部署（Vercel / Netlify）

```bash
# 1. 构建前端
cd frontend
npm run build

# 2. dist/ 文件夹即为部署产物

# 3. 配置环境变量
# VITE_API_BASE_URL=https://your-backend.railway.app
# VITE_WS_HOST=your-backend.railway.app
```

---

## 🔍 调试技巧

### 后端调试

```python
# 1. 启用详细日志
# 修改 .env
LOG_LEVEL=DEBUG

# 2. 添加断点（使用 pdb）
import pdb; pdb.set_trace()

# 3. 查看 SQL 查询
# 修改 database.py
engine = create_async_engine(
    settings.database_url,
    echo=True,  # 打印所有 SQL
)

# 4. WebSocket 调试
# 在 routers/command_ws.py 添加日志
logger.info(f"Received message: {message}")
```

### 前端调试

```typescript
// 1. 启用 React DevTools
// 浏览器安装 React Developer Tools 扩展

// 2. 调试 Zustand Store
// 在组件中添加
const store = useCommandStore();
console.log('Store state:', store);

// 3. WebSocket 消息日志
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.group(`📨 ${data.type}`);
  console.log('Data:', data.data);
  console.groupEnd();
};

// 4. 性能分析
// Chrome DevTools → Performance 标签
// 录制交互，查看组件渲染时间
```

---

## 📚 参考代码片段

### 示例：命令执行完整流程（前端）

```typescript
// 伪代码，展示完整交互
const handleVoiceCommand = async () => {
  // 1. 开始录音
  await audioCapture.startRecording();

  // 2. 连接 ASR WebSocket
  asrWs.connect();

  // 3. 发送音频流
  audioCapture.audioChunks.forEach(chunk => {
    asrWs.sendAudio(chunk);
  });

  // 4. 停止录音
  audioCapture.stopRecording();

  // 5. 获取转录文本（ASR 返回）
  const transcription = asrWs.finalText;

  // 6. 发送命令到后端
  commandWs.send({
    type: 'execute_command',
    user_input: transcription
  });

  // 7. 监听执行事件（自动更新 UI）
  // - llm_processing
  // - command_generated
  // - device_executed
  // - tts_ready

  // 8. 播放 TTS 音频
  audioPlayer.play(event.data.audio_base64);
};
```

### 示例：多客户端同步验证

```javascript
// 客户端 A 和 B 同时连接
// 客户端 A 执行命令
wsA.send({ type: 'execute_command', user_input: '设置柠檬香' });

// 客户端 B 自动收到事件
wsB.onmessage = (event) => {
  const { type, data } = JSON.parse(event.data);

  if (type === 'device_executed') {
    // ✅ 客户端 B 看到相同的设备状态
    console.log('设备状态已同步:', data);
  }
};
```

---

## ✅ 交接检查清单

在正式交给 Codex 之前，请确认：

- [ ] 后端服务器能成功启动（http://localhost:8000/health 返回 200）
- [ ] 数据库连接正常（能查询到 aromatherapy_commands 表）
- [ ] TTS API 能生成音频（curl 测试返回 MP3 文件）
- [ ] WebSocket 能连接（浏览器控制台测试无报错）
- [ ] 前端项目能编译（npm install 无错误）
- [ ] 所有配置文件已创建（package.json, tsconfig.json, vite.config.ts）
- [ ] 类型定义完整（types/aromatherapy.ts 无 TypeScript 错误）
- [ ] Git 仓库已初始化（.gitignore 配置正确）
- [ ] PROJECT_HANDOFF.md 文档已阅读

---

**准备就绪后，使用以下提示词开始：**

```
我已阅读 PROJECT_HANDOFF.md 和 EXECUTION_GUIDE.md。

当前项目状态:
- 后端: Phases 1-5 已完成 ✅
- 前端: Phase 6 部分完成（60%）

请继续 Phase 6，创建以下文件:
1. frontend/src/index.css（全局样式）
2. frontend/src/App.tsx（主应用组件）
3. frontend/src/pages/Dashboard.tsx（控制台页面）

技术要求已在 PROJECT_HANDOFF.md 第 6 节详细说明。

请一次性创建这 3 个文件的完整代码。
```
