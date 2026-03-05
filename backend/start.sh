#!/bin/bash

echo "========================================"
echo "AI 香薰控制系统 - 后端启动脚本"
echo "========================================"
echo ""

# 检查虚拟环境
if [ ! -d "venv" ]; then
    echo "[1/5] 创建虚拟环境..."
    python3 -m venv venv
else
    echo "[1/5] 虚拟环境已存在"
fi

# 激活虚拟环境
echo "[2/5] 激活虚拟环境..."
source venv/bin/activate

# 安装依赖
echo "[3/5] 安装依赖..."
pip install -q -r requirements.txt

# 检查 .env 文件
if [ ! -f ".env" ]; then
    echo "[4/5] 创建 .env 文件..."
    cp .env.example .env
    echo "警告: 请编辑 .env 文件配置数据库连接！"
    read -p "按任意键继续..."
else
    echo "[4/5] .env 文件已存在"
fi

# 启动服务器
echo "[5/5] 启动后端服务器..."
echo ""
echo "========================================"
echo "服务器运行在: http://localhost:8000"
echo "API 文档: http://localhost:8000/docs"
echo "按 Ctrl+C 停止服务器"
echo "========================================"
echo ""

python main.py
