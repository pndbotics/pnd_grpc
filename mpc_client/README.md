# Adam 上层运动控制客户端

上层运动控制基于 PNDbotics 机器人控制系统，通过 gRPC 调用底层运动控制接口，实现模式切换、站立动作、速度控制等功能。本目录提供 **Python** 与 **C++** 两种交互式客户端示例，以及可直接复用的 API 封装。

> **版本：v1.4.0** — 对齐 `proto/adam_control.proto`，补齐控制范式切换（`SetControlMode` / `GetControlState`）、连接检查与 C++/Python 行为一致性。

---

## 目录结构

```
Client/
├── proto/                  # gRPC 协议定义（权威来源）
├── include/                # protoc 生成的 C++ 头文件与源文件
├── src/
│   ├── adam_command.cpp    # C++ API 封装（可链接为 libadam_command）
│   └── adam_command_cli.cpp# C++ 交互式命令行客户端
├── python/
│   ├── adam_command_client.py
│   ├── requirements.txt
│   └── adam_control_pb2*.py  # protoc 生成的 Python 桩代码
├── ip_config.json          # 机器人服务端 IP 配置
├── build.sh                # 生成桩代码 + 编译 C++ 客户端
├── run.sh                  # 启动 Python 或 C++ 客户端
├── clean.sh                # 清理 build/bin/lib
└── README.md
```

---

## 接口定义（proto）

完整定义见 [`proto/adam_control.proto`](proto/adam_control.proto)。

```proto
service RobotControl {
  rpc SetMode (SetModeRequest) returns (SetModeResponse);
  rpc SetStandMotion (SetStandMotionRequest) returns (SetStandMotionResponse);
  rpc SetStandCarryBox (SetCarryBoxRequest) returns (SetCarryBoxResponse);
  rpc SetStandAction (SetActionRequest) returns (SetActionResponse);
  rpc SetStandDynamic (SetDynamicStandRequest) returns (SetDynamicStandResponse);
  rpc SetSpeed (SetSpeedRequest) returns (SetSpeedResponse);
  rpc AutoUnigaitCOM (SetUnigaitCOMRequest) returns (SetUnigaitCOMResponse);
  rpc SetErrorClear (SetErrorClearRequest) returns (SetErrorClearResponse);
  rpc GetStandList (GetStandListRequest) returns (GetStandListResponse);
  rpc GetRobotState (GetRobotStateRequest) returns (GetRobotStateResponse);
  rpc CloseProgram (CloseProgramRequest) returns (CloseProgramResponse);
  rpc SetControlMode (SetControlModeRequest) returns (SetControlModeResponse);
  rpc GetControlState (GetControlStateRequest) returns (GetControlStateResponse);
}
```

### 控制范式说明（v1.1.0 新增）

| domain_id | 含义 | 说明 |
|-----------|------|------|
| `0` | Traditional | 传统 MPC 控制 |
| `1` | RL | 强化学习控制 |
| `-1` | 未知 | 尚未收到底层 DDS `rt/control_mode_state` 反馈 |

- `SetControlMode`：向 DDS `rt/control_mode_cmd` 下发切换指令，服务端立即返回；硬件切换需数秒。
- `GetControlState`：读取 DDS `rt/control_mode_state` 的实时反馈。
- 客户端在 `SetControlMode` 后会**轮询** `GetControlState`（默认 30s 超时），与 Python/C++ 行为一致。

---

## 环境依赖

### 系统包（C++ 客户端）

```bash
# gRPC / Protobuf（若未安装，可从源码构建 v1.46.3）
sudo apt-get install -y protobuf-compiler libprotobuf-dev
sudo apt-get install -y nlohmann-json3-dev
# gRPC 通常需自行编译安装，或使用系统/第三方预编译包

# Python 桩代码生成
pip install -r python/requirements.txt
```

### Python 依赖

```bash
cd Client/python
pip install -r requirements.txt
```

仅需 `grpcio` 运行客户端；`grpcio-tools` 用于 `build.sh` 重新生成 Python 桩代码。

---

## 配置服务端 IP

编辑 [`ip_config.json`](ip_config.json)：

```json
{
  "server": {
    "ip": "192.168.x.x",
    "port": 6666,
    "comment": "ip 填写机器人网卡 IP；port 默认 6666，一般无需修改"
  }
}
```

**注意：**

- 修改 IP 后**无需重新编译** C++ 客户端（运行时读取配置）。
- 服务端 gRPC 端口固定为 **6666**，请与机器人端 `PndControl` 启动日志中的 `gRPC client connect address` 一致。
- 占位 IP（`xx.xx.xx.xx`、`0.0.0.0`、空）会在启动时被拒绝并提示配置。

### 连接前检查清单

1. 机器人端 `PndControl` 已启动，且编译时 `grpc_on=TRUE`（`buildrobot.sh` 中 `-Dgrpc_on=true`）。
2. 已执行 `install.sh` 并 `systemctl restart pnd_adam_dds.service` 使新二进制生效。
3. `ip_config.json` 中的 IP 与控制台打印的 gRPC 地址一致。
4. 客户端机器到机器人 **6666** 端口网络可达（防火墙未阻断）。

---

## 编译与运行

### 一键编译（C++ + 重新生成桩代码）

```bash
cd Client
chmod +x build.sh run.sh clean.sh
./build.sh
```

`build.sh` 会：

1. 根据 `proto/adam_control.proto` 生成 C++（`include/`）与 Python（`python/`）桩代码。
2. 在 `build/` 下 cmake 编译，输出可执行文件到 `bin/adam_command_client`。

若本机 `protoc` 版本与仓库内生成文件不同，可重复执行 `./build.sh` 覆盖本地桩代码，**不影响接口兼容性**。

### 启动客户端

```bash
./run.sh          # 默认启动 Python 客户端
./run.sh python   # 同上
./run.sh bin      # 启动 C++ 客户端（需先 build）
```

也可直接运行：

```bash
cd python && python3 adam_command_client.py
cd bin && ./adam_command_client
```

### 清理

```bash
./clean.sh   # 删除 build/、bin/、lib/，不删除 include/ 与 python/ 中的生成桩代码
```

---

## 交互式命令

两种客户端均支持 REPL 交互，输入 `help` 查看命令，输入 `exit` 退出。

| 命令 | 适用模式 | 说明 |
|------|----------|------|
| `SetMode` | 依 enable_list | 切换 `Start/Zero/Stand/Walk/Run/Stop` 等 |
| `SetStandMotion` | Stand | 预定义动作（如 `Greeting`） |
| `SetStandCarryBox` | Stand | 搬箱动作 |
| `SetStandAction` | Stand | 姿态 pitch/roll/yaw + 蹲起高度 |
| `SetStandDynamic` | Stand | 动态平衡开/关 |
| `SetSpeed` | Walk/Run | 设置 x/y/yaw 速度（注意安全） |
| `AutoUnigaitCOM` | Walk/Run | COM X 方向偏置平衡 |
| `SetErrorClear` | Stop | 清除驱动错误，无需关电 |
| `GetStandList` | 任意 | 获取固定动作/模式列表 |
| `GetRobotState` | 任意 | 获取当前状态与 **enable_list** |
| `SetControlMode` | 任意 | 切换 Traditional(0) / RL(1) |
| `GetControlState` | 任意 | 查询当前控制范式 |

**重要：** 下发控制指令前应先 `GetRobotState`，仅当目标模式/动作出现在对应 `*_enable_list` 中时才可执行。列表外的指令会被客户端或服务端拒绝。

### 姿态参数范围（SetStandAction）

| 参数 | 范围 |
|------|------|
| Pitch | [-0.1, 0.1] |
| Roll | [-0.06, 0.06] |
| Yaw | [-0.25, 0.25] |
| Base Height | [-0.2, 0.0] |

---

## API 调用示例

### Python

```python
import grpc
import adam_control_pb2
import adam_control_pb2_grpc

channel = grpc.insecure_channel("192.168.x.x:6666")
stub = adam_control_pb2_grpc.RobotControlStub(channel)

# 设置模式
resp = stub.SetMode(adam_control_pb2.SetModeRequest(mode="Stand"))
print(resp.success, resp.message)

# 获取机器人状态
state = stub.GetRobotState(adam_control_pb2.GetRobotStateRequest())
print(state.fsm_name, state.mode_enable_list)

# 切换控制范式并轮询确认（示例客户端已实现完整流程）
resp = stub.SetControlMode(adam_control_pb2.SetControlModeRequest(domain_id=1))
ctrl = stub.GetControlState(adam_control_pb2.GetControlStateRequest())
print(ctrl.domain_id)  # 0=Traditional, 1=RL
```

封装类方法（见 `python/adam_command_client.py`）：

```python
success, message = client.set_mode("Stand")
success, message = client.set_stand_motion("Greeting")
success, message = client.set_stand_action(0.0, 0.0, 0.0, -0.1)
ok, cur_mode, msg = client.set_control_mode(1)
ok, final_mode, msg = client.wait_for_control_mode(1, timeout_sec=30)
client.get_control_state()
success, message = client.get_robot_state(True)
```

### C++

```cpp
#include "adam_command.h"

adam_control::AdamCommand client("192.168.x.x:6666");

std::string message;
bool ok = client.SetMode("Stand", message);

int current_control_mode = -1;
bool motion_files_enable = false;
std::string fsm_name, current_motion, balance_control_enable;
std::vector<std::string> mode_enable_list, motion_enable_list, action_enable_list, carrybox_enable_list;
double stand_pitch, stand_roll, stand_yaw, stand_height, x_vel, y_vel, yaw_vel;
bool balance_control_state = false;
bool robot_state_flag = true;

ok = client.GetRobotState(robot_state_flag, fsm_name, current_motion,
                          mode_enable_list, motion_enable_list, action_enable_list, carrybox_enable_list,
                          balance_control_enable, stand_pitch, stand_roll, stand_yaw, stand_height,
                          x_vel, y_vel, yaw_vel, balance_control_state, motion_files_enable,
                          current_control_mode, message);

int final_mode = -1;
ok = client.SetControlModeAndWait(1, final_mode, message);

int domain_id = -1;
ok = client.GetControlState(domain_id, message);
```

可将 `src/adam_command.cpp` 编译为 `libadam_command` 链接到自有工程（见 `src/CMakeLists.txt`）。

---

## Python 与 C++ 客户端差异

| 特性 | Python | C++ |
|------|--------|-----|
| Tab 补全 | 支持 | 不支持 |
| 命令大小写 | 不敏感 | 敏感 |
| 连接超时检查 | 5s | 5s |
| SetControlMode 轮询 | 支持 | 支持 |
| GetControlState | 支持 | 支持 |
| 启动依赖 | pip 包 | 编译 + gRPC 库 |

推荐开发调试使用 **Python**；对性能或嵌入式集成有要求时使用 **C++ API**。

---

## 典型交互示例

### 模式切换

```
> GetRobotState
Current Mode: Start
Enable Mode List: ['Zero', 'Stand', ...]

> SetMode
Available Modes: Zero Stand
Enter parameter for SetMode: Stand
Success: Mode set successfully
```

### 控制范式切换

```
> GetControlState
Control State: domain_id=0 (Traditional)

> SetControlMode
Current control mode: 0 (Traditional)
Enter control mode (0/1): 1
  Waiting for hardware to switch to RL ...
  [2.5s] Current mode: 1 (RL)
Success: Switched to RL successfully.
```

### 连接失败

```
连接失败: 无法连接到 gRPC 服务 192.168.x.x:6666。请确认机器人端 PndControl 已启动...
```

或 C++：

```
Failed to get robot state: RPC failed: failed to connect to all addresses
```

---

## 二次开发建议

1. **以 `proto/adam_control.proto` 为唯一接口契约**；协议变更后执行 `./build.sh` 同步桩代码。
2. **始终先 `GetRobotState`** 再下发模式/动作类指令，避免状态机拒绝。
3. `SetSpeed` 存在安全风险，生产环境建议用手柄键控；客户端示例仅作接口演示。
4. `CloseProgram` 已在 proto 中定义，示例客户端未暴露；可按需自行封装。
5. 异步 API（`SetModeAsync` 等）在 `adam_command.h` 中提供，适用于非阻塞集成场景。

---

## 故障排查

| 现象 | 可能原因 | 处理 |
|------|----------|------|
| `failed to connect to all addresses` | IP/端口错误或 gRPC 未启用 | 检查 `ip_config.json`、PndControl 日志、防火墙 |
| `Invalid parameter` / enable_list 外指令 | 当前状态不允许 | `GetRobotState` 查看 `*_enable_list` |
| `SetControlMode` 超时 | 硬件切换较慢 | 用 `GetControlState` 继续确认；检查 DDS 服务 |
| C++ 编译找不到 nlohmann_json | 未安装 dev 包 | `sudo apt install nlohmann-json3-dev` |
| Python `ModuleNotFoundError: grpc` | 未安装依赖 | `pip install -r python/requirements.txt` |

---

## 变更记录

### v1.1.0

- C++ 补齐 `GetControlState`、`SetControlModeAndWait` 轮询逻辑，与 Python 对齐。
- C++ 启动时增加 gRPC 连接超时检查；修复 `SetStandDynamic` 误显示姿态范围的问题。
- 拆分 `adam_command_cli.cpp`，修复 CMake 重复编译同一源文件。
- `GetRobotState` 返回并展示 `current_control_mode`。
- 新增 `python/requirements.txt`；支持从 `ip_config.json` 读取 `port`。
- 全面更新本文档。

### v1.0.0

- 初始 Python / C++ 交互式客户端与基础 gRPC 接口封装。
