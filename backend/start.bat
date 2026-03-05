@echo off
echo ========================================
echo AI 香薰控制系统 - 后端启动脚本
echo ========================================
echo.

REM 检查虚拟环境
if not exist "venv\" (
    echo [1/5] 创建虚拟环境...
    python -m venv venv
) else (
    echo [1/5] 虚拟环境已存在
)

REM 激活虚拟环境
echo [2/5] 激活虚拟环境...
call venv\Scripts\activate

REM 安装依赖
echo [3/5] 安装依赖...
pip install -q -r requirements.txt

REM 检查 .env 文件
if not exist ".env" (
    echo [4/5] 创建 .env 文件...
    copy .env.example .env
    echo 警告: 请编辑 .env 文件配置数据库连接！
    pause
) else (
    echo [4/5] .env 文件已存在
)

REM 启动服务器
echo [5/5] 启动后端服务器...
echo.
echo ========================================
echo 服务器运行在: http://localhost:8000
echo API 文档: http://localhost:8000/docs
echo 按 Ctrl+C 停止服务器
echo ========================================
echo.

python main.py
