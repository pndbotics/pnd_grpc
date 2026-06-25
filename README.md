# PND 机器人上层运动控制 gRPC 客户端

本仓库面向 **PNDbotics（Adam）人形机器人** 的上层运动控制，通过 gRPC 调用机器人底层控制接口，实现模式切换、站立/动作控制、速度控制以及 **传统控制（MPC）** 与 **强化学习控制（RL）** 两种控制范式之间的切换。

机器人底层控制程序支持两套并行的上层控制服务，本仓库据此划分为两个相互独立的客户端目录：

| 目录 | 控制范式 | 服务包名 | gRPC 端口 | 客户端语言 |
|------|----------|----------|-----------|------------|
| [`mpc_client/`](mpc_client/) | **传统模式**（Traditional / MPC） | `adam_control` | `6666` | Python + C++ |
| [`rl_client/`](rl_client/) | **强化学习模式**（RL） | `pnd.robot` | `50051` | Python |

> 两个客户端均为**独立程序**，与机器人控制器分离部署。使用前请先确保机器人侧对应的控制服务已启动并正常运行，再启动客户端连接。

---

## 项目结构

```
pnd_grpc/
├── mpc_client/             # 传统模式（MPC）客户端
│   ├── proto/              # gRPC 协议定义（adam_control.proto，权威来源）
│   ├── include/            # protoc 生成的 C++ 头文件与源文件
│   ├── src/                # C++ API 封装 + 交互式命令行客户端
│   ├── python/             # Python 交互式客户端 + 生成的桩代码
│   ├── ip_config.json      # 机器人服务端 IP / 端口配置
│   ├── build.sh / run.sh / clean.sh
│   └── README.md           # 传统模式详细使用说明
│
├── rl_client/              # 强化学习模式（RL）客户端
│   ├── comm/
│   │   ├── proto/          # gRPC 协议定义（robot_control.proto）
│   │   └── grpc/           # protoc 生成的 Python 桩代码
│   ├── tools/grpc_client.py# Python 交互式客户端示例
│   └── README.md           # 强化学习模式详细使用说明
│
└── README.md               # 本文件（项目总览）
```

---

## 两种控制范式

机器人底层通过 DDS（`rt/control_mode_cmd` 下发、`rt/control_mode_state` 反馈）维护当前控制范式。两套客户端均提供 `SetControlMode` / `GetControlState` 接口用于切换与查询：

| domain_id | 含义 | 说明 |
|-----------|------|------|
| `0` | Traditional | 传统 MPC 控制 |
| `1` | RL | 强化学习控制 |
| `-1` | 未知 | 尚未收到底层 DDS `rt/control_mode_state` 反馈 |

- `SetControlMode`：向底层下发切换指令，服务端立即返回；硬件实际切换需数秒。
- `GetControlState`：读取底层实时反馈的控制范式。

> 控制范式（Traditional / RL）与“客户端目录”是两个独立的概念：每个客户端都能查询/切换控制范式，但两个目录面向的是机器人侧**不同的上层控制服务与接口契约**，端口与协议互不相同。

---

## 子项目一览

### 1. 传统模式客户端 `mpc_client/`

基于传统 MPC 控制的上层运动控制客户端（对齐 `proto/adam_control.proto`），同时提供 **Python** 与 **C++** 两种交互式客户端示例及可复用的 API 封装。

- **服务包**：`adam_control`，端口 **6666**（运行时从 `ip_config.json` 读取，修改 IP 无需重新编译 C++）。
- **核心接口**：`SetMode`、`SetStandMotion`、`SetStandCarryBox`、`SetStandAction`、`SetStandDynamic`、`SetSpeed`、`AutoUnigaitCOM`、`SetErrorClear`、`GetStandList`、`GetRobotState`、`CloseProgram`、`SetControlMode`、`GetControlState`。
- **典型能力**：模式切换（`Start/Zero/Stand/Walk/Run/Stop`）、站立预定义动作、搬箱、姿态（pitch/roll/yaw）与蹲起高度控制、动态平衡、行走/奔跑速度控制等。
- **快速开始**：

```bash
cd mpc_client
chmod +x build.sh run.sh clean.sh
./build.sh            # 生成桩代码 + 编译 C++ 客户端
./run.sh              # 默认启动 Python 客户端
./run.sh bin          # 启动 C++ 客户端（需先 build）
```

详见 [`mpc_client/README.md`](mpc_client/README.md)。

### 2. 强化学习模式客户端 `rl_client/`

基于强化学习（RL）控制的上层运动控制客户端（对齐 `comm/proto/robot_control.proto`），提供 Python 交互式客户端示例与开放 API，供用户自行构建客户端。

- **服务包**：`pnd.robot`，端口 **50051**（请勿修改）。
- **核心接口**：`SetMode`、`SetVelocity`、`SetHeight`、`SetMotion`、`SetTrackingMotion`、`GetRobotState`、`SetControlMode`、`GetControlState`、`Shutdown`。
- **FSM 状态**：`STOP`、`ZERO`、`STAND_WALK`、`MULTI_AGENT`、`MOTION_TRACK`、`JOG`、`VISION_TERRAIN`，支持上半身动作叠加（`MULTI_AGENT`）与全身轨迹跟踪（`MOTION_TRACK`）。
- **快速开始**：

```bash
pip install --user grpcio
# 本机
python3 rl_client/tools/grpc_client.py
# 远程真机（将 IP 替换为实际地址）
python3 rl_client/tools/grpc_client.py --addr 192.168.1.100:50051
```

> `robot_control_pb2_grpc.py` 内部以 `from comm.grpc import robot_control_pb2` 引用，须保持 `comm/grpc/` 目录结构，并将包含 `comm/` 的根目录加入 `PYTHONPATH`。

详见 [`rl_client/README.md`](rl_client/README.md)。

---

## 两套服务接口对照

| 维度 | 传统模式 `mpc_client` | 强化学习模式 `rl_client` |
|------|----------------------|--------------------------|
| 协议包名 | `adam_control` | `pnd.robot` |
| gRPC 端口 | `6666` | `50051` |
| 模式切换接口 | `SetMode`（`Start/Zero/Stand/Walk/Run/Stop`） | `SetMode`（`STOP/ZERO/STAND_WALK/MULTI_AGENT/MOTION_TRACK/...`） |
| 状态查询 | `GetRobotState`（含各类 `*_enable_list`） | `GetRobotState`（`switchable_states` / `available_actions`） |
| 动作播放 | `SetStandMotion` / `SetStandCarryBox` / `SetStandAction` | `SetMotion` / `SetTrackingMotion` |
| 速度/高度 | `SetSpeed` / `AutoUnigaitCOM` | `SetVelocity` / `SetHeight`（均为预留接口） |
| 控制范式切换 | `SetControlMode` / `GetControlState` | `SetControlMode` / `GetControlState` |
| 客户端语言 | Python + C++ | Python |
| 配置方式 | `ip_config.json`（IP/端口） | 命令行 `--addr` |

> 两套服务的 proto 接口契约不同，请以各自 `proto/` 目录下的定义文件为唯一权威来源。

---

## 通用使用约定

无论使用哪种客户端，请遵循以下约定：

1. **启动顺序**：先启动机器人侧对应的控制服务，再启动客户端。
2. **先查状态再下发**：下发模式/动作类指令前，先调用 `GetRobotState`，确认目标指令在对应的可执行列表（`*_enable_list` 或 `switchable_states` / `available_actions`）中。
3. **模式切换为异步**：`SetMode` 返回成功仅代表命令被接受，实际切换在后续控制周期完成，需轮询状态确认。
4. **控制范式切换需确认**：`SetControlMode` 后，硬件切换需要时间，建议通过 `GetControlState` 轮询确认。
5. **速度类接口注意安全**：传统模式 `SetSpeed` 存在安全风险，RL 模式 `SetVelocity` / `SetHeight` 当前为预留接口，行走速度建议使用手柄控制。

---

## 环境依赖

| 客户端 | 依赖 |
|--------|------|
| 传统模式 Python | `pip install -r mpc_client/python/requirements.txt`（运行仅需 `grpcio`） |
| 传统模式 C++ | `protobuf-compiler`、`libprotobuf-dev`、`nlohmann-json3-dev`、gRPC（v1.46.3） |
| 强化学习模式 Python | `pip install --user grpcio` |

---

## 更多信息

- 传统模式（MPC）完整接口、编译运行、交互命令、故障排查与变更记录：[`mpc_client/README.md`](mpc_client/README.md)
- 强化学习模式（RL）完整接口、FSM 状态说明、CLI 交互示例与封装调用：[`rl_client/README.md`](rl_client/README.md)
