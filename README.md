# TVLCOM：基于 TLV 帧格式的轻量通信框架

TVLCOM 是一个用 Python 实现的、基于 **自定义帧结构 + TLV（Type-Length-Value）** 的轻量通信小框架，主要用于：

- PC 上位机 与 嵌入式设备（MCU/开发板）之间的串口通信
- 自定义控制协议、参数配置协议
- 简单可靠的命令/数据交互

项目提供：

- 固定帧格式（Header + FrameID + TLV 数据 + CRC16 + Tail）
- TLV 构造与解析工具
- 字节流解析状态机
- 回调分发器（Dispatcher），带 ACK/NACK 机制
- 可插拔的传输层抽象（当前实现串口）

---

## 文档

更完整的协议与 API 文档见：

- `docs/TVLCOM.md`（English）
- `docs/TVLCOM_zh-CN.md`（中文）
- `docs/API.md`

---

## 安装与环境

- Python：建议 `>= 3.10`
- 核心依赖：`pyserial`

在项目根目录下：

```powershell
cd E:\PROJECT_Python\TVLCOM

python -m venv .venv
.\.venv\Scripts\activate

pip install -e .
```

---

## 快速开始

运行串口示例：

```powershell
cd E:\PROJECT_Python\TVLCOM
python main.py
```

> 需要先在 `main.py` 中把 `port="COM3"` 修改为你实际的串口号。

---

## 许可证

项目使用 GPLv3 许可证，详情见仓库中的 `LICENSE` 文件。
