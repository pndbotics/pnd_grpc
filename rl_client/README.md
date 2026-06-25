# PND Robot gRPC 客户端使用说明

上层运动控制通过 gRPC 接口实现。用户可参考内置客户端示例 `tools/grpc_client.py` 进行机器人控制，也可基于开放 API 自行构建客户端。

**客户端为独立程序**，与机器人控制器分离部署。请先确保机器人控制程序已启动并正常运行，再启动客户端连接。

- 默认服务端口：`50051`（请勿修改）
- 服务端暂不支持二次开发

---

## 接口定义（Proto）

```proto
syntax = "proto3";
package pnd.robot;

service RobotControl {
  rpc SetMode         (SetModeRequest)         returns (SetModeResponse);
  rpc SetVelocity     (SetVelocityRequest)     returns (SetVelocityResponse);
  rpc SetHeight       (SetHeightRequest)       returns (SetHeightResponse);
  rpc SetMotion       (SetMotionRequest)       returns (SetMotionResponse);
  rpc SetTrackingMotion (SetTrackingMotionRequest) returns (SetTrackingMotionResponse);
  rpc GetRobotState   (GetRobotStateRequest)   returns (GetRobotStateResponse);
  rpc SetControlMode  (SetControlModeRequest)  returns (SetControlModeResponse);
  rpc GetControlState (GetControlStateRequest) returns (GetControlStateResponse);
  rpc Shutdown        (ShutdownRequest)        returns (ShutdownResponse);
}

message SetModeRequest {
  string target_state = 1;
}

message SetModeResponse {
  bool success = 1;
  string message = 2;
  string current_state = 3;
}

message SetVelocityRequest {
  double vx = 1;       // 前进/后退，归一化 [-1.0, 1.0]
  double vy = 2;       // 左移/右移
  double vyaw = 3;     // 转向
}

message SetVelocityResponse {
  bool success = 1;
  string message = 2;
}

message SetHeightRequest {
  double height = 1;   // 归一化 [-1.0, 1.0]，0 为默认站高
}

message SetHeightResponse {
  bool success = 1;
  string message = 2;
}

message SetMotionRequest {
  enum Command {
    PLAY = 0;
    STOP = 1;
  }
  Command command = 1;
  string motion_file = 2;
}

message SetMotionResponse {
  bool success = 1;
  string message = 2;
  string current_motion = 3;
  bool is_playing = 4;
}

message SetTrackingMotionRequest {
  string motion_file = 1;
}

message SetTrackingMotionResponse {
  bool success = 1;
  string message = 2;
  string current_tracking_motion = 3;
}

message GetRobotStateRequest {}

message GetRobotStateResponse {
  bool success = 1;
  string fsm_state = 2;
  double vx = 3;
  double vy = 4;
  double vyaw = 5;
  double height = 6;
  string current_motion_file = 7;
  bool motion_playing = 8;
  string current_tracking_motion = 9;
  bool tracking_playing = 10;
  repeated string switchable_states = 11;
  repeated string available_actions = 12;
}

message SetControlModeRequest {
  int32 domain_id = 1;    // 0=Traditional, 1=RL
}

message SetControlModeResponse {
  bool success = 1;
  string message = 2;
}

message GetControlStateRequest {}

message GetControlStateResponse {
  bool success = 1;
  string message = 2;
  int32 domain_id = 3;    // 0=Traditional, 1=RL；-1 表示尚未收到 state
}

message ShutdownRequest {
  bool force = 1;
}

message ShutdownResponse {
  bool success = 1;
  string message = 2;
}
```

---

## 客户端说明

### 依赖安装

客户端最小依赖：

```bash
pip install --user grpcio
```

### 客户端文件

独立客户端需携带以下 API 文件（已随仓库提供，可直接使用）：

```
comm/
├── __init__.py
└── grpc/
    ├── robot_control_pb2.py        # 消息定义
    └── robot_control_pb2_grpc.py   # 服务 Stub
tools/grpc_client.py                # Python 交互式客户端示例（可选）
```

> `robot_control_pb2_grpc.py` 内部以 `from comm.grpc import robot_control_pb2` 方式引用，因此须**保持 `comm/grpc/` 目录结构**，并把包含 `comm/` 的根目录加入 `PYTHONPATH`（或在代码中 `sys.path.insert`）。

用户可参考 `tools/grpc_client.py` 复写客户端，或基于上述 API 文件自行构建。

### 连接地址

连接前请确认机器人控制程序已启动。客户端通过 `--addr` 指定机器人主机地址：

| 场景 | 连接地址 |
|------|----------|
| 本机 | `localhost:50051` |
| 远程真机 | `<机器人 IP>:50051` |

> 端口固定为 `50051`，请勿修改。

### 启动客户端

```bash
# 本机
python3 tools/grpc_client.py

# 远程机器人（将 IP 替换为实际地址）
python3 tools/grpc_client.py --addr 192.168.1.100:50051
```

若连接失败，终端会提示：

```
[ERROR] RPC failed: UNAVAILABLE - failed to connect to all addresses
```

请确认机器人控制程序已启动，且 IP 与网络连通正常。

---

## 接口调用说明

下发指令前，应先调用 `GetRobotState` 获取 `switchable_states`（可切换状态）和 `available_actions`（当前可用接口），不满足条件的指令将无法执行。

### Python 接入示例

```python
import sys
from pathlib import Path

# 将包含 comm/ 的根目录加入路径（按实际位置修改）
sys.path.insert(0, str(Path(__file__).resolve().parent))

import grpc
from comm.grpc import robot_control_pb2 as pb2
from comm.grpc import robot_control_pb2_grpc as pb2_grpc

channel = grpc.insecure_channel("192.168.1.100:50051")
stub = pb2_grpc.RobotControlStub(channel)
```

---

### GetRobotState

+ 方法说明：查询机器人当前状态，可在任意时刻调用。
+ 注意事项：下发其他指令前应优先调用此方法，根据 `switchable_states` 和 `available_actions` 判断指令是否可执行。

```python
state = stub.GetRobotState(pb2.GetRobotStateRequest())
print(state.fsm_state)              # 当前 FSM 状态
print(list(state.switchable_states)) # 可切换状态列表
print(list(state.available_actions)) # 当前可用接口列表
```

---

### SetMode

+ 方法说明：切换机器人 FSM 状态。状态名：`STOP`、`ZERO`、`STAND_WALK`、`MULTI_AGENT`、`MOTION_TRACK`、`JOG`、`VISION_TERRAIN`。
+ 注意事项：
  - 只能切换到 `switchable_states` 中的状态，否则返回 `success=false`；
  - 返回 `success=true` 仅表示**命令已接受**，实际切换在后续控制周期异步执行；
  - 特别地，在 `ZERO` 状态下调用 `SetMode("STAND_WALK")` 会被接受（`success=true`），但 FSM 会等零位归零动作完成后才真正切换，可通过轮询 `GetRobotState().fsm_state` 确认是否到达。

```python
resp = stub.SetMode(pb2.SetModeRequest(target_state="STAND_WALK"))
# resp.success 为执行结果，resp.message 为结果信息
```

典型流程：`STOP → ZERO → STAND_WALK → MULTI_AGENT / MOTION_TRACK / ...`

---

### SetMotion

+ 方法说明：在 `MULTI_AGENT` 状态下播放或停止上半身动作文件。
+ 注意事项：
  - 仅当 `available_actions` 包含 `SetMotion` 时可调用；
  - `PLAY` 需提供 `motion_file` 路径，且文件必须为 `.txt` 后缀并真实存在于**机器人侧**，否则返回 `success=false`；
  - 相对路径以**机器人侧仓库根目录**为基准解析（如 `extra/offline_motion/Greeting.txt`），也可传入绝对路径。

```python
# 播放
resp = stub.SetMotion(pb2.SetMotionRequest(
    command=pb2.SetMotionRequest.PLAY,
    motion_file="extra/offline_motion/Greeting.txt",
))

# 停止
resp = stub.SetMotion(pb2.SetMotionRequest(
    command=pb2.SetMotionRequest.STOP,
))
```

---

### SetTrackingMotion

+ 方法说明：在 `MOTION_TRACK` 状态下切换轨迹跟踪文件。
+ 注意事项：仅当 `available_actions` 包含 `SetTrackingMotion` 时可调用。

```python
resp = stub.SetTrackingMotion(pb2.SetTrackingMotionRequest(
    motion_file="path/to/trajectory.txt",
))
```

---

### SetVelocity

+ 方法说明：设置行走速度（`vx`, `vy`, `vyaw`），归一化范围 `[-1.0, 1.0]`。
+ 注意事项：**当前为预留接口，暂未接入**。行走速度请使用手柄控制。

```python
resp = stub.SetVelocity(pb2.SetVelocityRequest(vx=0.5, vy=0.0, vyaw=0.0))
```

---

### SetHeight

+ 方法说明：设置站立高度，归一化范围 `[-1.0, 1.0]`，`0` 为默认站高。
+ 注意事项：**当前为预留接口，暂未接入**。

```python
resp = stub.SetHeight(pb2.SetHeightRequest(height=-0.1))
```

---

### SetControlMode

+ 方法说明：切换控制范式。`domain_id=0` 为传统控制，`domain_id=1` 为 RL 控制。
+ 注意事项：切换后建议调用 `GetControlState` 确认是否生效。

```python
resp = stub.SetControlMode(pb2.SetControlModeRequest(domain_id=0))
```

---

### GetControlState

+ 方法说明：查询当前控制范式。
+ 注意事项：`domain_id=-1` 表示尚未收到状态反馈。

```python
resp = stub.GetControlState(pb2.GetControlStateRequest())
print(resp.domain_id)  # 0=Traditional, 1=RL
```

---

### Shutdown

+ 方法说明：请求关闭机器人控制器。

```python
resp = stub.Shutdown(pb2.ShutdownRequest(force=False))
```

---

## 接口一览表

| 接口名称 | 描述 | 调用参数 | 赋值示例 |
|----------|------|----------|----------|
| `GetRobotState` | 查询机器人状态 | 无 | — |
| `SetMode` | 切换 FSM 状态 | `target_state` | `"ZERO"`, `"STAND_WALK"`, `"MULTI_AGENT"`, `"STOP"` |
| `SetMotion` | 播放/停止上半身动作 | `command`, `motion_file` | `PLAY`, `"extra/offline_motion/Greeting.txt"` |
| `SetTrackingMotion` | 切换轨迹文件 | `motion_file` | `"path/to/traj.txt"` |
| `SetVelocity` | 设置行走速度（预留） | `vx`, `vy`, `vyaw` | `0.5, 0.0, 0.0` |
| `SetHeight` | 设置站高（预留） | `height` | `-0.1` |
| `SetControlMode` | 切换控制范式 | `domain_id` | `0` 或 `1` |
| `GetControlState` | 查询控制范式 | 无 | — |
| `Shutdown` | 关闭控制器 | `force` | `false` |

---

## FSM 状态与可用接口

| 状态 | 说明 | 可用接口（`available_actions`） |
|------|------|----------------------------------|
| `STOP` | 急停 | （无） |
| `ZERO` | 零位校准 | （无） |
| `STAND_WALK` | 基础站立行走 | `SetVelocity` |
| `MULTI_AGENT` | 上半身动作叠加 | `SetVelocity`, `SetHeight`, `SetMotion` |
| `MOTION_TRACK` | 全身轨迹跟踪 | `SetVelocity`, `SetTrackingMotion` |
| `JOG` | 慢跑 | `SetVelocity` |
| `VISION_TERRAIN` | 视觉地形 | `SetVelocity` |

> 从 `STAND_WALK` 可进入哪些策略态（`MULTI_AGENT` / `MOTION_TRACK` / `JOG` / `VISION_TERRAIN`）由机器人侧授权列表决定，实际可切换目标以 `GetRobotState` 返回的 `switchable_states` 为准。

---

## CLI 客户端交互示例

`tools/grpc_client.py` 提供交互式命令行客户端，支持 Tab 补全、命令历史，并根据机器人当前状态动态显示可用命令。

```bash
python3 tools/grpc_client.py --addr 192.168.1.100:50051
```

```
  Connected: 192.168.1.100:50051  |  FSM: STOP

╔══════════════════════════════════════════╗
║   PND Robot Control Client v1.0          ║
║   Type 'help' for commands, Tab to complete ║
╚══════════════════════════════════════════╝

robot> help
  Current FSM: STOP

  Global commands:
    state              Query current robot state
    mode [STATE]       Switch FSM mode (Tab for options)
    controlmode <0|1>  Set control paradigm (0=Traditional, 1=RL)
    controlstate       Query control paradigm from DDS state
    shutdown           Shutdown controller
    clear              Clear screen
    quit / exit        Exit client

robot> state
  FSM State:         STOP
  Velocity:          vx=0.000  vy=0.000  vyaw=0.000
  Height:            0.000
  Motion File:       (none)
  Motion Playing:    False
  Tracking Motion:   (none)
  Tracking Playing:  False
  Switchable:        ZERO
  Available Actions: (none)

robot> mode ZERO
  [OK] ok  (current=STOP)

robot> mode STAND_WALK
  [OK] ok  (current=ZERO)
# 命令已接受；待零位归零动作完成后，FSM 才会自动切到 STAND_WALK

robot> mode MULTI_AGENT
  [OK] ok  (current=STAND_WALK)

robot> motion play extra/offline_motion/Greeting.txt
  [OK] ok  (file=.../Greeting.txt, playing=True)
  Observed: motion_file=.../Greeting.txt, playing=True

robot> motion stop
  [OK] ok
  Observed: motion_file=(none), playing=False

robot> controlmode 0
  [OK] ok, Traditional (0) queued for control_mode_cmd

robot> controlstate
  [OK] ok, current control mode Traditional  domain_id=0

robot> quit
  Bye.
```

> 启动横幅由 `intro` 输出，`Connected` 行在创建客户端时先于横幅打印，因此实际顺序如上。

---

## 封装与脚本化调用

交互式 CLI 适合手动调试。若需在自己的程序中集成，或做一次性的命令行调用（自动化场景），可参考以下封装。

### 封装类

```python
import grpc
from comm.grpc import robot_control_pb2 as pb2
from comm.grpc import robot_control_pb2_grpc as pb2_grpc


class PndRobotClient:
    def __init__(self, addr: str = "localhost:50051"):
        self._stub = pb2_grpc.RobotControlStub(grpc.insecure_channel(addr))

    def get_state(self):
        return self._stub.GetRobotState(pb2.GetRobotStateRequest())

    def set_mode(self, target_state: str):
        r = self._stub.SetMode(pb2.SetModeRequest(target_state=target_state))
        return r.success, r.message

    def play_motion(self, file_path: str):
        r = self._stub.SetMotion(pb2.SetMotionRequest(
            command=pb2.SetMotionRequest.PLAY, motion_file=file_path))
        return r.success, r.message

    def stop_motion(self):
        r = self._stub.SetMotion(pb2.SetMotionRequest(
            command=pb2.SetMotionRequest.STOP))
        return r.success, r.message

    def set_tracking_motion(self, file_path: str):
        r = self._stub.SetTrackingMotion(pb2.SetTrackingMotionRequest(motion_file=file_path))
        return r.success, r.message

    def set_control_mode(self, domain_id: int):
        r = self._stub.SetControlMode(pb2.SetControlModeRequest(domain_id=domain_id))
        return r.success, r.message
```

### 命令行入口（一次性调用）

“启动时指定参数、执行单条命令”的方式，便于脚本和自动化集成：

```python
import argparse

def main():
    parser = argparse.ArgumentParser(description="PND Robot gRPC one-shot client")
    parser.add_argument("--addr", default="localhost:50051", help="机器人地址 host:port")
    parser.add_argument("--state", action="store_true", help="查询机器人状态")
    parser.add_argument("--mode", help="切换 FSM 状态，如 ZERO / STAND_WALK")
    parser.add_argument("--motion-play", metavar="PATH", help="播放上半身动作文件")
    parser.add_argument("--motion-stop", action="store_true", help="停止动作播放")
    parser.add_argument("--tracking", metavar="PATH", help="切换轨迹跟踪文件")
    parser.add_argument("--control-mode", type=int, choices=[0, 1], help="0=传统, 1=RL")
    args = parser.parse_args()

    client = PndRobotClient(args.addr)

    if args.state:
        s = client.get_state()
        print(f"fsm_state={s.fsm_state}")
        print(f"switchable_states={list(s.switchable_states)}")
        print(f"available_actions={list(s.available_actions)}")
    if args.mode:
        print("set_mode:", client.set_mode(args.mode))
    if args.motion_play:
        print("play_motion:", client.play_motion(args.motion_play))
    if args.motion_stop:
        print("stop_motion:", client.stop_motion())
    if args.tracking:
        print("set_tracking_motion:", client.set_tracking_motion(args.tracking))
    if args.control_mode is not None:
        print("set_control_mode:", client.set_control_mode(args.control_mode))


if __name__ == "__main__":
    main()
```

### 命令行参数

| 参数 | 说明 | 赋值示例 |
|------|------|----------|
| `--addr` | 机器人地址 `host:port` | `192.168.1.100:50051` |
| `--state` | 查询机器人状态 | （无值） |
| `--mode` | 切换 FSM 状态 | `ZERO`、`STAND_WALK` |
| `--motion-play` | 播放上半身动作文件 | `extra/offline_motion/Greeting.txt` |
| `--motion-stop` | 停止动作播放 | （无值） |
| `--tracking` | 切换轨迹跟踪文件 | `path/to/traj.txt` |
| `--control-mode` | 切换控制范式 | `0` 或 `1` |

### 调用示例

```bash
# 查询状态
python3 my_client.py --addr 192.168.1.100:50051 --state

# 切换到 ZERO（零位校准）
python3 my_client.py --addr 192.168.1.100:50051 --mode ZERO

# 在 MULTI_AGENT 状态下播放动作
python3 my_client.py --addr 192.168.1.100:50051 --motion-play extra/offline_motion/Greeting.txt
```

> 提示：下发动作 / 模式指令前请先用 `--state` 确认 `switchable_states` 与 `available_actions`，避免命令被服务端拒绝。

---

## 注意事项

1. **启动顺序**：先启动机器人控制程序，再启动客户端。
2. **先查状态再下发**：通过 `GetRobotState` 检查 `switchable_states` 和 `available_actions`。
3. **模式切换异步**：`SetMode` 成功后需轮询 `fsm_state` 确认是否到达目标状态。
4. **零位校准**：`ZERO → STAND_WALK` 的 `SetMode` 会被接受，但切换会延迟到零位归零动作完成后自动生效，请轮询 `fsm_state` 确认。
5. **速度/站高未开放**：`SetVelocity` / `SetHeight` 为预留接口，请使用手柄控制运动。
