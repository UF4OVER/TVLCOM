# TVLCOM 性能分析与优化报告

> 目标：在不改变公共 API（含历史兼容行为）且单元测试全绿的前提下，降低“高频小帧/字节流”场景的 CPU 与分配开销。

## 1. 基线与瓶颈定位方法

### 建议的分析组合
- **Micro benchmark**：`benchmarks/bench_protocol.py`
  - 覆盖 CRC、TLV 解析、build+parse 往返、tracemalloc 峰值
- **CPU Profile**：`benchmarks/profile_cpu.py`（生成 `benchmarks/profile.pstats`）
  - 用 `pstats` 排序 `cumtime` / `tottime` 看热点
- **内存分析**：`tracemalloc`（已在 micro bench 中）

### 当前项目的主要热路径（从 profile 观测）
- `PYTVLCOM.RE.TLVParser.process_byte`：流式逐字节状态机，是最大耗时来源
- CRC 更新：每字节 CRC 更新如果有 `bytes` 拼接/创建，会导致大量分配/GC
- `parse_tlvs`：解析 TLV payload（通常在一帧结束时调用）

> 经验结论：协议类库在“串口/网络逐字节 feed”场景下，最大的优化空间通常来自 **减少每字节 Python 层开销** 与 **减少临时对象分配**。

## 2. 已落地的改进（代码级）

### 2.1 修复并优化 CRC/构帧（`PYTVLCOM/COM.py`）
- 修复了 `create_raw_entry`/CRC table/`crc16_ccitt` 等段落曾出现的代码断裂问题
- 新增 `crc16_ccitt_update(crc, chunk)`：支持 **分块增量** CRC
- `build_frame` 改为分两段计算 CRC（FrameID+Len，再 payload），避免 `bytes([..]) + payload` 临时拼接
- ACK/NACK TLV 前缀提前构造，减少 Dispatcher 热路径分配
- `create_int32_entry` 保持兼容：同时支持
  - 新式：`create_int32_entry(value, tlv_type=...)`
  - 旧式：`create_int32_entry(tlv_type, value)`

### 2.2 TLV 解析（`PYTVLCOM/RE.py`）
- `parse_tlvs` 使用 `memoryview` 解析，减少切片中间对象
- `TLVParser` CRC 校验改为纯 int 的 **零分配 per-byte 更新**：
  - `TLVParser._crc16_update_byte(value, b)` 使用 `_CRC16_TABLE` 直接更新
  - 避免 `crc16_ccitt(bytes([fid, dlen]) + data)` 这种拼接
  - 避免每字节 `bytes([ch])` 分配

## 3. 性能数据（本机一次实际运行结果）

> 说明：不同机器/解释器版本会有差异，建议以你自己的 CI/目标设备为准复测。

### Micro benchmark（`python benchmarks\\bench_protocol.py`）
最新一次观测输出：
- `crc_time_s`: **0.916s**（20000 次）
- `parse_tlvs_time_s`: **0.177s**（20000 次，每次 10 个 TLV）
- `frame_roundtrip_time_s`: **0.467s**（5000 次 build + byte-by-byte parse）
- `mem_peak_kb`: **~9.82 KB**（tracemalloc）

### CPU profile（`benchmarks/profile_cpu.py`）
`pstats` Top（节选）：
- `RE.py:process_byte`（156 万次调用）为第一大热点
- `_crc16_update_byte`（144 万次调用）占用显著减少（已从“每次 import/创建 bytes”变为纯运算）

## 4. 算法与复杂度分析（关键函数）

### `parse_tlvs(buf)`
- 时间复杂度：**O(n)**，n = payload 字节数
- 空间复杂度：**O(k + payload拷贝)**，k = TLV 数量
  - 当前实现会为每个 value 创建 `bytes` 拷贝（保证 `TLVEntry.value` 是稳定的 `bytes`）
  - 如果你允许 value 为 `memoryview`（零拷贝），可进一步降内存/CPU，但会改变公共语义

### `TLVParser.process_byte(ch)`
- 时间复杂度：**O(1)**/byte
- 但常量项非常关键：每多一次分配、属性查找、函数调用都会被“放大”到百万级

### `crc16_ccitt(data)`
- 时间复杂度：**O(n)**
- 表驱动减少常量项；增量更新可避免额外的 `+` 拼接

## 5. 进一步的优化建议（可选，按收益/风险排序）

### A. 解析吞吐提升（高收益，中等改动）
1. **提供按块 feed 的 API**：比如 `process_bytes(data: bytes)`
   - 串口/网络读取通常是一块一块来的；逐字节 Python 循环是主要瓶颈
   - 可以在内部用 `for b in data:`，并把状态机分支优化成局部变量/局部绑定

2. `parse_tlvs` 零拷贝变体
   - 新增 `parse_tlvs_view(buf)->list[TLVEntryView(type:int, value:memoryview)]`
   - 或给 `TLVEntry` 增加 `memoryview` 可选字段

### B. CRC 加速（中收益，低风险）
- Python 里 CRC 计算可考虑使用 `binascii.crc_hqx`（CRC-CCITT X25/不同 init/poly 需要核对）
- 或提供可选加速依赖（C 扩展 / Rust）

### C. IO/资源管理（取决于你的上层应用）
- 串口读取建议：批量读取（例如 `read(size)` 或 `read_all()`）并喂给 parser
- 确保使用 `with` / context manager 管理串口/文件
- 如果传输层需要高吞吐，考虑后台线程 + queue 聚合（减少主线程频繁 wakeup）

## 6. 代码审查与规范工具建议

- 已有 ruff dev 依赖（`pyproject.toml`），建议再加：
  - `pytest`（如果你的 dev 环境里还没固定）
  - `mypy`（若你希望类型更严格）

Ruff 常用：
- `ruff check .`
- `ruff format .`（如启用格式化）

## 7. 测试与验证

- 单元测试：`python -m pytest -q`（本次修改后全绿：8 passed）
- 基准：`python benchmarks\\bench_protocol.py`
- Profile：`python benchmarks\\profile_cpu.py` + pstats

---

### 变更摘要
- **修复**：`COM.py` 中 TLV/CRC 相关实现曾出现的断裂/拼接错误
- **优化**：避免热路径 bytes 拼接与 per-byte 分配
- **验证**：单测通过 + 基准/profile 可复现

