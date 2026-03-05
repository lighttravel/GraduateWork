# 项目交接文档 - AI 香薰控制系统

## 📋 项目概述

### 项目名称
AI-Powered Voice-Controlled Aromatherapy System (AI 语音控制香薰系统)

### 项目目标
构建一个完整的 Web 应用，通过自然语言语音控制香薰设备。用户说话 → 语音识别 → AI 理解意图 → 生成控制指令 → 可视化展示 → 语音反馈。

### 核心功能流程
```
用户语音输入
    ↓
科大讯飞 ASR (语音识别)
    ↓
GLM-4.7 LLM (意图解析)
    ↓
生成 JSON 控制指令
    ↓
虚拟设备执行 + 可视化展示
    ↓
科大讯飞 TTS (语音反馈)
```

---

## 🏗️ 技术架构

### 后端技术栈
- **Framework**: FastAPI (Python 3.11+)
- **Database**: PostgreSQL 15+ (with JSONB support)
- **ORM**: SQLAlchemy (async)
- **LLM**: GLM-4.7 (via Anthropic-compatible API)
- **ASR**: 科大讯飞 WebSocket API
- **TTS**: 科大讯飞 WebSocket API
- **Real-time**: WebSocket (native FastAPI)

### 前端技术栈
- **Framework**: React 18 + TypeScript
- **Build Tool**: Vite 5
- **State Management**: Zustand
- **HTTP Client**: Axios
- **WebSocket**: Native WebSocket API
- **Charts**: Recharts
- **Routing**: React Router v6

### 项目结构
```
E:\graduatework\test1\
├── backend/                    # ✅ 后端完成 (Phases 1-5)
│   ├── main.py                 # FastAPI 应用入口
│   ├── config.py               # 配置管理
│   ├── database.py             # 数据库连接
│   ├── models.py               # 数据模型
│   ├── .env                    # 环境变量
│   ├── requirements.txt        # Python 依赖
│   ├── migrations/             # 数据库迁移
│   ├── repositories/           # 数据访问层
│   ├── services/               # 业务逻辑层
│   ├── routers/                # API 路由
│   ├── prompts/                # LLM 系统提示词
│   └── tests/                  # 测试文件
└── frontend/                   # 🚧 前端进行中 (Phase 6 部分完成)
    ├── index.html              # ✅ HTML 入口
    ├── package.json            # ✅ 依赖配置
    ├── tsconfig.json           # ✅ TypeScript 配置
    ├── vite.config.ts          # ✅ Vite 配置
    └── src/
        ├── main.tsx            # ✅ React 入口
        ├── App.tsx             # ⏳ 需要创建
        ├── index.css           # ⏳ 需要创建
        ├── types/              # ✅ 类型定义
        ├── services/           # ✅ API 客户端
        ├── store/              # ✅ Zustand 状态管理
        ├── components/         # ⏳ 需要创建组件
        ├── hooks/              # ⏳ 需要创建自定义 Hooks
        └── pages/              # ⏳ 需要创建页面
```

---

## ✅ 已完成工作（Phases 1-5）

### Phase 1: 后端基础设施 ✅
**文件列表**:
- `backend/main.py` - FastAPI 应用，已集成 CORS、生命周期管理
- `backend/config.py` - Pydantic Settings 配置类
- `backend/database.py` - 异步 SQLAlchemy 引擎和会话工厂
- `backend/models.py` - Pydantic 和 SQLAlchemy 数据模型
- `backend/requirements.txt` - 所有 Python 依赖
- `backend/.env` - 环境变量（包含所有 API 密钥）
- `backend/migrations/001_initial_schema.sql` - PostgreSQL 建表脚本
- `backend/repositories/command_repository.py` - Repository 模式数据访问

**核心功能**:
- ✅ FastAPI 应用启动
- ✅ PostgreSQL 连接池管理
- ✅ 环境变量验证
- ✅ 健康检查端点: `GET /health`

### Phase 2: LLM 意图解析 ✅
**文件列表**:
- `backend/services/llm_service.py` - GLM-4.7 客户端封装
- `backend/services/intent_parser.py` - 自然语言 → JSON 解析器
- `backend/prompts/aromatherapy_system_prompt.txt` - LLM 系统提示词

**核心功能**:
- ✅ Anthropic SDK 集成（GLM-4.7 兼容接口）
- ✅ 自动重试 + 指数退避
- ✅ JSON 提取和 Pydantic 验证
- ✅ 默认响应生成

**API 凭证（已配置在 .env）**:
```
ANTHROPIC_AUTH_TOKEN=daf6f0cdfc6a49f79fcc3496bb379605.1W23CEHM12iYFaU4
ANTHROPIC_BASE_URL=https://open.bigmodel.cn/api/anthropic
```

### Phase 3: 科大讯飞 ASR 集成 ✅
**文件列表**:
- `backend/services/iflytek_asr.py` - ASR WebSocket 客户端
- `backend/routers/asr_ws.py` - WebSocket 端点 `/api/ws/asr`

**核心功能**:
- ✅ HMAC-SHA256 签名认证
- ✅ 实时音频流传输（PCM 16kHz）
- ✅ 部分和最终转录结果回调
- ✅ 错误处理和重连逻辑

**API 凭证（已配置在 .env）**:
```
IFLYTEK_ASR_APPID=9ed12221
IFLYTEK_ASR_API_SECRET=NmYwODk4ODVlNGE2YWZhNGM2YjhmMjE4
IFLYTEK_ASR_API_KEY=b1ffeca6c122160445ebd4a4d69003b4
```

### Phase 4: 科大讯飞 TTS 集成 ✅
**文件列表**:
- `backend/services/iflytek_tts.py` - TTS WebSocket 客户端
- `backend/routers/tts.py` - HTTP 端点 `/api/tts` 和 `/api/tts/voices`

**核心功能**:
- ✅ HMAC-SHA256 签名认证
- ✅ 多种语音选项（xiaoyan, aisjiuxu 等）
- ✅ 音频格式：MP3（base64 或二进制）
- ✅ 速度和音量可调

**API 凭证**（同 ASR，已配置在 .env）

### Phase 5: 命令执行管道 ✅
**文件列表**:
- `backend/services/device_controller.py` - 设备控制抽象层
- `backend/services/command_executor.py` - 端到端命令编排器
- `backend/routers/command_ws.py` - WebSocket 端点 `/api/ws/commands`

**核心功能**:
- ✅ **硬件抽象层**: `DeviceController` 接口（为未来 ESP32 集成做准备）
- ✅ `VirtualDeviceController`: 当前可视化模式实现
- ✅ 5 步执行流水线: ASR → LLM → DB → 设备 → TTS
- ✅ **多客户端同步**: WebSocket 房间广播
- ✅ 11 种实时事件类型

**WebSocket 事件类型**:
```typescript
'llm_processing'       // LLM 正在处理
'command_generated'    // 生成了控制指令
'command_saved'        // 保存到数据库
'device_executing'     // 设备正在执行
'device_executed'      // 设备执行完成
'tts_generating'       // 正在生成语音
'tts_ready'           // 语音准备就绪
'execution_complete'   // 完整流程完成
'execution_error'      // 执行错误
'device_status'        // 设备状态更新
'device_stopped'       // 设备停止
```

---

## 🚧 当前进度（Phase 6）

### Phase 6: 前端基础设施（部分完成）

#### ✅ 已创建文件
1. `frontend/package.json` - npm 依赖配置
2. `frontend/tsconfig.json` - TypeScript 编译配置
3. `frontend/tsconfig.node.json` - Vite 配置的 TS 支持
4. `frontend/vite.config.ts` - Vite 构建配置（已配置代理）
5. `frontend/index.html` - HTML 入口文件
6. `frontend/src/main.tsx` - React 入口
7. `frontend/src/types/aromatherapy.ts` - 完整的 TypeScript 类型定义
8. `frontend/src/services/apiClient.ts` - Axios HTTP 客户端 + WebSocket 管理器
9. `frontend/src/store/commandStore.ts` - Zustand 全局状态管理

#### ⏳ 待创建文件
1. `frontend/src/App.tsx` - 主应用组件（路由、布局）
2. `frontend/src/index.css` - 全局样式
3. `frontend/src/pages/Dashboard.tsx` - 主控制台页面
4. `frontend/src/components/` - 所有 UI 组件（下面列出）
5. `frontend/src/hooks/` - 自定义 React Hooks（下面列出）

---

## 📝 后续任务详解（Phases 7-13）

### Phase 7: 语音采集组件 ⏳

#### 需要创建的文件

**1. `frontend/src/hooks/useAudioCapture.ts`**
```typescript
// 功能: 使用 MediaRecorder API 捕获麦克风音频
// 返回: { startRecording, stopRecording, audioBlob, isRecording, error }
// 音频格式: PCM 16kHz 16bit mono
// 注意: 需要处理浏览器权限请求
```

**2. `frontend/src/hooks/useASRWebSocket.ts`**
```typescript
// 功能: 管理 /api/ws/asr 的 WebSocket 连接
// 发送: 音频二进制流
// 接收: { type: 'partial' | 'final' | 'error', text: string }
// 返回: { connect, disconnect, sendAudio, transcription, isConnected }
```

**3. `frontend/src/components/VoiceCapture.tsx`**
```tsx
// UI 组件:
// - 按住说话按钮（onMouseDown 开始录音，onMouseUp 停止）
// - 实时显示转录文本（partial + final）
// - 录音状态指示器（红色圆点动画）
// - 错误提示
```

#### 实现要求
- **音频格式**: 必须是 PCM 16kHz 16bit mono（iFlytek 要求）
- **按钮交互**: 只支持"按住说话"，不支持唤醒词
- **流式传输**: 音频块实时发送到 WebSocket，不要等录音结束
- **错误处理**: 捕获麦克风权限拒绝、WebSocket 连接失败

---

### Phase 8: 可视化仪表盘 ⏳

#### 需要创建的文件

**1. `frontend/src/hooks/useCommandWebSocket.ts`**
```typescript
// 功能: 管理 /api/ws/commands 的 WebSocket 连接
// 发送消息:
//   - { type: 'execute_command', user_input: string }
//   - { type: 'get_status' }
//   - { type: 'stop_device' }
// 接收: 11 种事件（见上面的事件类型列表）
// 自动更新 Zustand store
```

**2. `frontend/src/components/AromatherapyCharts.tsx`**
```tsx
// 使用 Recharts 库创建:
// 1. 强度柱状图 (BarChart) - 显示 intensity (1-10)
// 2. 混合比例饼图 (PieChart) - 显示 mixing_ratios
// 3. 持续时间进度条 - 显示 remaining_minutes / duration_minutes
// 4. 释放节奏动画 - 根据 release_rhythm 显示不同动画
```

**3. `frontend/src/components/CommandPanel.tsx`**
```tsx
// 显示内容:
// - 当前用户输入文本
// - LLM 响应文本
// - JSON 控制指令（格式化显示）
// - 执行状态（当前步骤）
// - 示例输入（"把房间调成偏清新的感觉"）
```

**4. `frontend/src/components/DeviceStatus.tsx`**
```tsx
// 显示设备当前状态:
// - 是否激活 (is_active)
// - 当前香型 (current_scent) - 图标 + 文字
// - 当前强度 (current_intensity) - 数字 + 进度条
// - 剩余时间 (remaining_minutes) - 倒计时
// - 错误信息（如果有）
```

#### 实现要求
- **实时更新**: 所有组件通过 Zustand store 自动更新
- **响应式设计**: 适配桌面和平板（手机可选）
- **颜色方案**:
  - lemon: 黄色 #FFD700
  - lavender: 紫色 #E6E6FA
  - woody: 棕色 #8B4513
  - floral: 粉色 #FFB6C1
- **动画**: 使用 CSS transitions，避免过度动画

---

### Phase 9: 音频播放组件 ⏳

#### 需要创建的文件

**1. `frontend/src/hooks/useAudioPlayback.ts`**
```typescript
// 功能: 使用 HTML5 Audio API 播放 TTS 音频
// 输入: base64 编码的 MP3 音频
// 返回: { play, pause, stop, isPlaying, currentTime, duration }
// 注意: 处理浏览器自动播放策略（需要用户交互后才能播放）
```

**2. `frontend/src/components/AudioPlayer.tsx`**
```tsx
// UI 组件:
// - 播放/暂停按钮
// - 音量滑块 (0-100)
// - 播放进度条
// - 当前时间 / 总时长显示
// 自动播放: 收到 tts_ready 事件后自动播放
```

#### 实现要求
- **音频来源**: WebSocket 事件 `tts_ready.data.audio_base64`
- **格式转换**: `data:audio/mpeg;base64,${audio_base64}`
- **自动播放**: 仅在用户有交互后（点击按钮后）允许自动播放
- **错误处理**: 播放失败时显示错误，但不阻塞界面

---

### Phase 10: 实时多客户端同步 ⏳

#### 任务
这部分**后端已完成**（Phase 5 中的 WebSocket 房间广播）。

前端需要做的:
1. 在 `useCommandWebSocket.ts` 中监听所有广播事件
2. 更新 Zustand store（已在 Phase 8 中实现）
3. 测试多个浏览器标签同时打开，验证状态同步

#### 验证方法
```bash
# 打开两个浏览器窗口/标签页
窗口 A: http://localhost:5173
窗口 B: http://localhost:5173

# 在窗口 A 执行语音命令
# 验证窗口 B 自动更新相同状态
```

---

### Phase 11: 数据收集与导出 ⏳

#### 需要创建的后端文件

**1. `backend/routers/feedback.py`**
```python
# PATCH /api/commands/{command_id}/feedback
# Request: { "user_feedback": 1-5 }
# 更新数据库中的 user_feedback 字段
```

**2. `backend/routers/export.py`**
```python
# GET /api/export/training-data?format=csv|jsonl
# 导出所有命令数据（用于模型微调）
# 包含: user_input_text, control_json, user_feedback
# 过滤: 只导出有 feedback 的数据
```

#### 需要创建的前端文件

**1. `frontend/src/components/FeedbackButton.tsx`**
```tsx
// 5 星评分组件
// 点击星星后调用 PATCH /api/commands/{id}/feedback
// 显示提交状态（成功/失败）
```

**2. `frontend/src/components/ExportButton.tsx`**
```tsx
// 导出按钮
// 点击后下载 CSV/JSONL 文件
// 使用 window.location.href 或 fetch + Blob
```

---

### Phase 12: 错误处理与边缘情况 ⏳

#### 需要创建的后端文件

**1. `backend/middleware/error_handlers.py`**
```python
# FastAPI 全局异常处理器
# - ValidationError → 400 JSON 响应
# - DatabaseError → 500 JSON 响应
# - TimeoutError → 504 JSON 响应
# 统一错误格式: { "error": "...", "detail": "..." }
```

#### 需要创建的前端文件

**1. `frontend/src/components/ErrorBoundary.tsx`**
```tsx
// React Error Boundary
// 捕获组件树中的 JavaScript 错误
// 显示友好的错误页面（不崩溃）
```

**2. `frontend/src/components/ConnectionStatus.tsx`**
```tsx
// WebSocket 连接状态指示器
// - 绿色圆点: 已连接
// - 红色圆点: 已断开
// - 黄色圆点: 正在重连
```

#### 需要处理的边缘情况
1. **网络断开**: 显示"连接已断开"提示，自动重连
2. **LLM 返回无效 JSON**: 显示错误，提示重试
3. **麦克风权限拒绝**: 显示"需要麦克风权限"提示
4. **TTS 生成失败**: 仍然显示文本响应，不阻塞流程
5. **数据库连接失败**: 后端返回 503，前端显示"服务暂时不可用"

---

### Phase 13: 测试（80%+ 覆盖率）⏳

#### 后端测试文件

**1. `backend/tests/test_intent_parser.py`**
```python
# 单元测试 intent_parser.parse_command()
# Mock LLM 响应，测试 JSON 解析
# 测试用例:
#   - 正常 JSON
#   - 缺少字段
#   - 类型错误
#   - 混合香型验证
```

**2. `backend/tests/test_command_executor.py`**
```python
# 集成测试命令执行流水线
# Mock: LLM, ASR, TTS, 数据库
# 测试用例:
#   - 成功执行完整流程
#   - LLM 失败时的错误处理
#   - 设备执行失败时的状态更新
```

**3. `backend/tests/test_device_controller.py`**
```python
# 单元测试 VirtualDeviceController
# 测试用例:
#   - execute_command() 更新状态
#   - get_device_status() 返回正确值
#   - stop() 停止设备
```

#### 前端测试文件

**1. `frontend/src/hooks/__tests__/useAudioCapture.test.ts`**
```typescript
// Mock MediaRecorder API
// 测试录音开始/停止
// 测试错误处理（权限拒绝）
```

**2. `frontend/src/components/__tests__/VoiceCapture.test.tsx`**
```typescript
// 使用 @testing-library/react
// 测试按钮点击交互
// 测试转录文本显示
```

**3. `frontend/src/services/__tests__/apiClient.test.ts`**
```typescript
// Mock axios
// 测试 HTTP 请求拦截器
// 测试 WebSocket 连接管理
```

#### E2E 测试

**1. `e2e/test_voice_command_flow.spec.ts`**
```typescript
// 使用 Playwright
// 模拟完整用户流程:
//   1. 打开页面
//   2. 点击"按住说话"按钮
//   3. 发送模拟音频
//   4. 验证转录显示
//   5. 验证可视化更新
//   6. 验证 TTS 播放
```

---

## 🎯 继续执行指南

### 给 Codex 的提示词模板

```markdown
我正在开发一个 AI 语音控制香薰系统。后端已完成（Phases 1-5），前端基础设施已搭建（Phase 6 部分完成）。

当前状态:
- ✅ 后端: FastAPI + PostgreSQL + LLM + ASR + TTS 全部完成
- ✅ 前端: 项目配置、类型定义、API 客户端、状态管理已完成
- ⏳ 待完成: React 组件、页面、Hooks

请继续 Phase [阶段编号]，创建 [文件名]。

技术要求:
- React 18 + TypeScript (严格模式)
- 状态管理: 使用 Zustand store (已定义在 src/store/commandStore.ts)
- API 调用: 使用 src/services/apiClient.ts 中的 apiClient 和 WebSocketManager
- 类型定义: 使用 src/types/aromatherapy.ts 中的类型
- 代码风格: 函数组件 + Hooks，immutable 数据更新

参考已有文件:
- 类型定义: frontend/src/types/aromatherapy.ts
- 状态管理: frontend/src/store/commandStore.ts
- API 客户端: frontend/src/services/apiClient.ts

请创建 [具体文件路径]，包含完整实现和注释。
```

---

### 分阶段执行计划

#### 立即执行（Phase 6 剩余部分）

**任务 1: 创建 App.tsx 和 index.css**
```
提示词: "继续 Phase 6，创建 frontend/src/App.tsx（包含 React Router 和主布局）和 frontend/src/index.css（全局样式）"
```

**任务 2: 创建 Dashboard 页面**
```
提示词: "继续 Phase 6，创建 frontend/src/pages/Dashboard.tsx（主控制台页面，集成所有组件的容器）"
```

#### 下一步（Phase 7）

**任务 3: 创建音频采集 Hook**
```
提示词: "开始 Phase 7，创建 frontend/src/hooks/useAudioCapture.ts（使用 MediaRecorder API 捕获麦克风音频，格式 PCM 16kHz）"
```

**任务 4: 创建 ASR WebSocket Hook**
```
提示词: "继续 Phase 7，创建 frontend/src/hooks/useASRWebSocket.ts（管理 /api/ws/asr WebSocket 连接，实时发送音频流）"
```

**任务 5: 创建语音采集组件**
```
提示词: "继续 Phase 7，创建 frontend/src/components/VoiceCapture.tsx（按住说话按钮 UI，实时显示转录文本）"
```

#### 后续阶段依次执行 Phase 8 → 9 → 10 → 11 → 12 → 13

---

## 📊 进度跟踪清单

```
[✅] Phase 1: 后端基础设施
[✅] Phase 2: LLM 意图解析
[✅] Phase 3: iFlytek ASR 集成
[✅] Phase 4: iFlytek TTS 集成
[✅] Phase 5: 命令执行管道
[🚧] Phase 6: 前端基础设施 (60% 完成)
    [✅] package.json
    [✅] tsconfig.json
    [✅] vite.config.ts
    [✅] 类型定义
    [✅] API 客户端
    [✅] 状态管理
    [⏳] App.tsx
    [⏳] index.css
    [⏳] Dashboard.tsx
[⏳] Phase 7: 语音采集组件
    [⏳] useAudioCapture.ts
    [⏳] useASRWebSocket.ts
    [⏳] VoiceCapture.tsx
[⏳] Phase 8: 可视化仪表盘
    [⏳] useCommandWebSocket.ts
    [⏳] AromatherapyCharts.tsx
    [⏳] CommandPanel.tsx
    [⏳] DeviceStatus.tsx
[⏳] Phase 9: 音频播放组件
    [⏳] useAudioPlayback.ts
    [⏳] AudioPlayer.tsx
[⏳] Phase 10: 多客户端同步（后端已完成，前端测试）
[⏳] Phase 11: 数据收集与导出
    [⏳] backend/routers/feedback.py
    [⏳] backend/routers/export.py
    [⏳] FeedbackButton.tsx
    [⏳] ExportButton.tsx
[⏳] Phase 12: 错误处理
    [⏳] error_handlers.py
    [⏳] ErrorBoundary.tsx
    [⏳] ConnectionStatus.tsx
[⏳] Phase 13: 测试（80%+ 覆盖率）
    [⏳] 后端单元测试
    [⏳] 前端组件测试
    [⏳] E2E 测试
```

---

## 🔑 关键配置信息

### 环境变量（backend/.env）
```env
# 数据库
DATABASE_URL=postgresql+asyncpg://user:password@localhost:5432/aromatherapy_db

# 科大讯飞（ASR + TTS）
IFLYTEK_ASR_APPID=9ed12221
IFLYTEK_ASR_API_SECRET=NmYwODk4ODVlNGE2YWZhNGM2YjhmMjE4
IFLYTEK_ASR_API_KEY=b1ffeca6c122160445ebd4a4d69003b4
IFLYTEK_TTS_APPID=9ed12221
IFLYTEK_TTS_API_SECRET=NmYwODk4ODVlNGE2YWZhNGM2YjhmMjE4
IFLYTEK_TTS_API_KEY=b1ffeca6c122160445ebd4a4d69003b4

# GLM-4.7 LLM
ANTHROPIC_AUTH_TOKEN=daf6f0cdfc6a49f79fcc3496bb379605.1W23CEHM12iYFaU4
ANTHROPIC_BASE_URL=https://open.bigmodel.cn/api/anthropic

# 应用配置
APP_ENV=development
CORS_ORIGINS=http://localhost:5173,http://localhost:3000
```

### API 端点列表

#### HTTP 端点
- `GET /health` - 健康检查
- `POST /api/tts` - TTS 语音合成
- `GET /api/tts/voices` - 获取可用语音列表
- `PATCH /api/commands/{id}/feedback` - 提交用户反馈（待实现）
- `GET /api/export/training-data` - 导出训练数据（待实现）

#### WebSocket 端点
- `WS /api/ws/asr` - 实时语音识别
- `WS /api/ws/commands` - 命令执行和设备状态同步

---

## 🚨 重要约束和设计决策

### 必须遵守的规则
1. **不使用唤醒词**: 只支持"按住说话"交互
2. **单用户系统**: 无需身份认证
3. **多客户端同步**: 所有客户端必须看到相同设备状态
4. **硬件抽象**: 使用 DeviceController 接口，当前实现为 VirtualDeviceController
5. **不可变数据**: 前端状态更新必须使用不可变模式（Zustand 要求）

### 音频格式要求
- **ASR 输入**: PCM 16kHz 16bit mono
- **TTS 输出**: MP3 格式（base64 或二进制）

### 数据模型核心字段
```typescript
ControlJson {
  scent_type: 'lemon' | 'lavender' | 'woody' | 'floral' | 'mixed'
  intensity: 1-10
  duration_minutes: 5-120
  release_rhythm: 'gradual' | 'pulse' | 'intermittent'
  mixing_ratios?: { lemon?: 0-1, lavender?: 0-1, woody?: 0-1, floral?: 0-1 }
}
```

---

## 📞 如果遇到问题

### 常见问题排查

1. **后端启动失败**
   - 检查 PostgreSQL 是否运行
   - 验证 .env 文件中的 DATABASE_URL
   - 运行 `pip install -r requirements.txt`

2. **前端编译错误**
   - 运行 `npm install`
   - 检查 Node.js 版本（需要 20+）
   - 验证 TypeScript 类型导入路径

3. **WebSocket 连接失败**
   - 确认后端运行在 `http://localhost:8000`
   - 检查 Vite 代理配置（vite.config.ts）
   - 查看浏览器控制台的 WebSocket 错误

4. **API 调用失败**
   - 检查 CORS 配置（backend/main.py）
   - 验证 API 密钥是否正确
   - 查看后端日志输出

---

## 🎓 学习资源

- **FastAPI**: https://fastapi.tiangolo.com/
- **React + TypeScript**: https://react.dev/learn/typescript
- **Zustand**: https://docs.pmnd.rs/zustand/
- **Recharts**: https://recharts.org/
- **科大讯飞 ASR API**: https://www.xfyun.cn/doc/asr/rtasr/API.html
- **科大讯飞 TTS API**: https://www.xfyun.cn/doc/tts/online_tts/API.html

---

## 📄 完整文件清单

### 后端文件（已完成）
```
backend/
├── main.py ✅
├── config.py ✅
├── database.py ✅
├── models.py ✅
├── requirements.txt ✅
├── .env ✅
├── .env.example ✅
├── migrations/
│   └── 001_initial_schema.sql ✅
├── repositories/
│   ├── __init__.py ✅
│   └── command_repository.py ✅
├── services/
│   ├── __init__.py ✅
│   ├── llm_service.py ✅
│   ├── intent_parser.py ✅
│   ├── iflytek_asr.py ✅
│   ├── iflytek_tts.py ✅
│   ├── device_controller.py ✅
│   └── command_executor.py ✅
├── routers/
│   ├── __init__.py ✅
│   ├── asr_ws.py ✅
│   ├── tts.py ✅
│   ├── command_ws.py ✅
│   ├── feedback.py ⏳
│   └── export.py ⏳
├── middleware/
│   ├── __init__.py ✅
│   └── error_handlers.py ⏳
├── prompts/
│   └── aromatherapy_system_prompt.txt ✅
└── tests/
    ├── __init__.py ✅
    ├── test_intent_parser.py ⏳
    ├── test_command_executor.py ⏳
    └── test_device_controller.py ⏳
```

### 前端文件（部分完成）
```
frontend/
├── index.html ✅
├── package.json ✅
├── tsconfig.json ✅
├── tsconfig.node.json ✅
├── vite.config.ts ✅
└── src/
    ├── main.tsx ✅
    ├── App.tsx ⏳
    ├── index.css ⏳
    ├── types/
    │   └── aromatherapy.ts ✅
    ├── services/
    │   └── apiClient.ts ✅
    ├── store/
    │   └── commandStore.ts ✅
    ├── hooks/
    │   ├── useAudioCapture.ts ⏳
    │   ├── useASRWebSocket.ts ⏳
    │   ├── useCommandWebSocket.ts ⏳
    │   └── useAudioPlayback.ts ⏳
    ├── components/
    │   ├── VoiceCapture.tsx ⏳
    │   ├── AromatherapyCharts.tsx ⏳
    │   ├── CommandPanel.tsx ⏳
    │   ├── DeviceStatus.tsx ⏳
    │   ├── AudioPlayer.tsx ⏳
    │   ├── FeedbackButton.tsx ⏳
    │   ├── ExportButton.tsx ⏳
    │   ├── ErrorBoundary.tsx ⏳
    │   └── ConnectionStatus.tsx ⏳
    └── pages/
        └── Dashboard.tsx ⏳
```

---

**总结**: 这份文档包含了项目的完整上下文、已完成工作、待办任务、技术要求和执行步骤。你可以直接将相关章节复制给 Codex，它就能无缝接手继续开发。每个阶段都有明确的文件列表、功能描述和技术要求。
