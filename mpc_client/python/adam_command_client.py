import grpc
import adam_control_pb2
import adam_control_pb2_grpc
import cmd
import json
import os
import socket
import time

GRPC_CONNECT_TIMEOUT_SEC = 5
CONTROL_MODE_POLL_TIMEOUT_SEC = 60  # RL 初始化可能需 10–30s，留足余量
CONTROL_MODE_POLL_INTERVAL_SEC = 0.5


def control_mode_name(mode):
    if mode == -1:
        return "Unknown (not yet received)"
    if mode == 0:
        return "Traditional"
    if mode == 1:
        return "RL"
    return f"Unknown ({mode})"


def is_port_open(host, port, timeout_sec=1.0):
    try:
        with socket.create_connection((host, port), timeout=timeout_sec):
            return True
    except OSError:
        return False


def build_connection_hint(server_address):
    host, _, port = server_address.partition(":")
    port = port or "6666"
    hints = [
        "请确认：1) 机器人端 PndControl 已启动且编译时 grpc_on=TRUE（buildrobot.sh 中 -Dgrpc_on=true）；",
        "2) 已执行 install.sh 并 systemctl restart pnd_adam_dds.service 使新二进制生效；",
        f"3) ip_config.json 中的 IP 与 PndControl 启动日志里的 gRPC client connect address 一致；",
        f"4) 端口 {port} 未被防火墙阻断。",
    ]
    if host and not is_port_open(host, int(port)):
        hints.insert(
            1,
            f"当前 {host}:{port} 端口不可达（PndControl 可能未启用 gRPC 或未重启）；",
        )
    return "".join(hints)


def wait_for_grpc_channel(channel, timeout_sec=GRPC_CONNECT_TIMEOUT_SEC):
    try:
        grpc.channel_ready_future(channel).result(timeout=timeout_sec)
        return True
    except grpc.FutureTimeoutError:
        return False


class AdamCommand(cmd.Cmd):
    intro = "Adam Command Client v1.1.0\nType 'help' for usage information.\n"
    prompt = "> "

    def __init__(self, server_address):
        super().__init__()
        self.server_address = server_address
        self.channel = grpc.insecure_channel(
            server_address,
            options=[
                ("grpc.enable_http_proxy", 0),
                ("grpc.keepalive_time_ms", 10000),
                ("grpc.keepalive_timeout_ms", 5000),
            ],
        )
        self.stub = adam_control_pb2_grpc.RobotControlStub(self.channel)
        self.state = "WAIT_COMMAND"
        self.pending_command = ""
        self.enable_list = []
        self.fsm_name = ""
        self.mode_enable_list = []
        self.motion_enable_list = []
        self.action_enable_list = []
        self.carrybox_enable_list = []

        if not wait_for_grpc_channel(self.channel):
            raise ConnectionError(
                f"无法连接到 gRPC 服务 {server_address}。"
                f"{build_connection_hint(server_address)}"
            )

        print(f"已连接到 gRPC 服务: {self.server_address}")
        self.get_robot_state(False)

    def do_exit(self, arg):
        """Exit the Adam Command Client."""
        if self.state == "WAIT_COMMAND":
            print("Exiting Adam Command Client.")
            return True
        else:
            print("Exiting current command interface.")
            self.state = "WAIT_COMMAND"
            self.prompt = "> "
            self.lastcmd = None
            return False

    def do_clear(self, arg):
        """Clear the terminal screen."""
        os.system("cls" if os.name == "nt" else "clear")
        self.lastcmd = None

    def do_help(self, arg):
        """Show available commands."""
        print("Available commands:")
        print("  SetMode")
        print("  SetStandMotion")
        print("  SetStandCarryBox")
        print("  SetStandAction")
        print("  SetStandDynamic")
        print("  SetSpeed")
        print("  AutoUnigaitCOM")
        print("  SetErrorClear")
        print("  GetStandList")
        print("  GetRobotState")
        print("  SetControlMode  <0=Traditional|1=RL>  -- queue mode switch; poll GetControlState to confirm")
        print("  GetControlState                       -- hardware mode from DDS rt/control_mode_state (-1=unknown)")
        print("  clear")
        print("  exit")
        self.lastcmd = None

    def default(self, line):
        """Handle default command input."""
        normalized_line = line.upper()
        if self.state == "WAIT_COMMAND":
            self.handle_command(normalized_line)
        elif self.state == "WAIT_PARAMETER":
            self.handle_parameter(line)
        elif self.state == "WAIT_SPEED_INPUT":
            self.handle_speed_input(line)
        elif self.state == "WAIT_ACTION_INPUT":
            self.handle_action_input(line)
        else:
            print(f"Unknown command: {line}")

    def handle_command(self, input_line):
        """Handle commands in WAIT_COMMAND state."""
        if input_line == "EXIT":
            return self.do_exit(None)
        elif input_line == "HELP":
            self.do_help(None)
        elif input_line in [cmd.upper() for cmd in self.supported_commands]:
            self.get_robot_state(False)
            self.pending_command = input_line

            if input_line == "SETMODE":
                self.enable_list = self.mode_enable_list
                print("Available Modes: ", ", ".join(self.enable_list))
                self.state = "WAIT_PARAMETER"
                self.prompt = (
                    f"Enter parameter for {self.pending_command.capitalize()}: "
                )
            elif input_line == "SETSTANDMOTION" or input_line == "SETSTANDCARRYBOX":
                if self.fsm_name != "Stand":
                    print(
                        f"Current mode is {self.fsm_name}, this command can only be executed when mode is 'Stand'."
                    )
                    return
                if input_line == "SETSTANDMOTION":
                    self.enable_list = self.motion_enable_list
                    print("Available Motions: ", ", ".join(self.enable_list))
                else:
                    self.enable_list = self.carrybox_enable_list
                    print("Available Carry Boxes: ", ", ".join(self.enable_list))
                self.state = "WAIT_PARAMETER"
                self.prompt = (
                    f"Enter parameter for {self.pending_command.capitalize()}: "
                )
            elif input_line == "SETSTANDACTION":
                if self.fsm_name != "Stand":
                    print(
                        f"Current mode is {self.fsm_name}, this command can only be executed when mode is 'Stand'."
                    )
                    return
                print(
                    f"Current action values (stand_pitch stand_roll stand_yaw base_height) ({self.stand_pitch}, {self.stand_roll}, {self.stand_yaw}, {self.stand_height})"
                )
                print("Please enter values within the following ranges:")
                print("  - Pitch: [-0.1, 0.1]")
                print("  - Roll: [-0.06, 0.06]")
                print("  - Yaw: [-0.25, 0.25]")
                print("  - Base Height: [-0.2, 0.0]")
                self.state = "WAIT_ACTION_INPUT"
                self.prompt = "Action> Enter action values (stand_pitch stand_roll stand_yaw stand_height): "
            elif input_line == "SETSTANDDYNAMIC":
                if self.fsm_name != "Stand":
                    print(
                        f"Current mode is {self.fsm_name}, this command can only be executed when mode is 'Stand'."
                    )
                    return
                self.state = "WAIT_PARAMETER"
                print(f"Dynamic Stand State: {self.balance_control_state}.")
                self.prompt = "Enter 'true' or 'false' for enable balance: "
            elif input_line == "SETSPEED":
                if self.fsm_name not in ["Walk", "Run"]:
                    print(
                        f"Current mode is {self.fsm_name}, this command can only be executed when mode is 'Walk' or 'Run'."
                    )
                    return
                self.state = "WAIT_SPEED_INPUT"
                self.prompt = "Speed> Enter speed values (x y yaw): "
            elif input_line == "AUTOUNIGAITCOM":
                if self.fsm_name not in ["Walk", "Run"]:
                    print(
                        f"Current mode is {self.fsm_name}, this command can only be executed when mode is 'Walk' or 'Run'."
                    )
                    return
                self.state = "WAIT_PARAMETER"
                self.prompt = "Enter 'true' or 'false' for unigait COM: "
            elif input_line == "SETERRORCLEAR":
                if self.fsm_name != "Stop":
                    print(
                        f"Current mode is {self.fsm_name}, this command can only be executed when mode is 'Stop'."
                    )
                    return
                success, message = self.set_error_clear(True)
                if success:
                    print(f"Success: {message}")
                else:
                    print(f"Failed: {message}")
            elif input_line == "GETSTANDLIST":
                self.get_stand_list()
            elif input_line == "GETROBOTSTATE":
                self.get_robot_state(True)
            elif input_line == "GETCONTROLSTATE":
                self.get_control_state()
            elif input_line == "SETCONTROLMODE":
                current_mode = getattr(self, 'current_control_mode', -1)
                print(f"Current control mode: {current_mode} ({control_mode_name(current_mode)})")
                print("Enter control mode (0=Traditional MPC, 1=RL): ")
                self.pending_command = "SETCONTROLMODE"
                self.state = "WAIT_PARAMETER"
                self.prompt = "Enter control mode (0/1): "
        else:
            print(f"Unknown command: {input_line}")
        self.lastcmd = None

    def handle_parameter(self, input_line):
        if input_line == "exit":
            self.state = "WAIT_COMMAND"
            self.prompt = "> "
            self.lastcmd = None
            return

        if self.pending_command in ["SETMODE", "SETSTANDMOTION", "SETSTANDCARRYBOX"]:
            if input_line not in self.enable_list:
                print(
                    "Error: Invalid parameter. Please enter a valid option from the enable list."
                )
                return

        if self.pending_command == "SETCONTROLMODE":
            try:
                domain_id = int(input_line)
                if domain_id not in [0, 1]:
                    raise ValueError
            except ValueError:
                print("Error: Invalid control mode. Please enter 0 (Traditional) or 1 (RL).")
                self.state = "WAIT_COMMAND"
                self.prompt = "> "
                self.lastcmd = None
                return
            _do_set_control_mode(self, domain_id)
            self.state = "WAIT_COMMAND"
            self.prompt = "> "
            self.lastcmd = None
            return

        if self.pending_command in ["SETSTANDDYNAMIC", "AUTOUNIGAITCOM"]:
            if input_line.lower() not in ["true", "false"]:
                print("Error: Invalid parameter. Please enter 'true' or 'false'.")
                return

            # handle SetStandDynamic command
            if self.pending_command == "SETSTANDDYNAMIC":
                current_state = self.balance_control_state  # get current state
                new_state = input_line.lower() == "true"  # user input

                if new_state == current_state:
                    print(f"Already in the state: {current_state}. No change needed.")
                    # wait input
                    return

        tokens = [self.pending_command, input_line]
        success, message = execute_command(self, tokens)
        if success:
            print(f"Success: {message}")
        else:
            print(f"Failed: {message}")
        self.state = "WAIT_COMMAND"
        self.prompt = "> "
        self.lastcmd = None

    def handle_speed_input(self, input_line):
        """Handle speed input in WAIT_SPEED_INPUT state."""
        if input_line == "exit":
            self.state = "WAIT_COMMAND"
            self.prompt = "> "
            self.lastcmd = None
            return

        tokens = input_line.split()
        if len(tokens) != 3:
            print(
                "Error: Invalid number of arguments for SetSpeed. Must be three float values."
            )
            return
        try:
            x_speed = float(tokens[0])
            y_speed = float(tokens[1])
            yaw_speed = float(tokens[2])
            success, message = self.set_speed(x_speed, y_speed, yaw_speed)
            if success:
                print(f"Success: {message}")
            else:
                print(f"Failed: {message}")
        except ValueError:
            print("Error: Invalid parameter for SetSpeed. Must be three float values.")
        self.lastcmd = None

    def handle_action_input(self, input_line):
        """Handle action input in WAIT_ACTION_INPUT state."""
        if input_line == "exit":
            self.state = "WAIT_COMMAND"
            self.prompt = "> "
            self.lastcmd = None
            return

        tokens = input_line.split()
        if len(tokens) != 4:
            print(
                "Error: Invalid number of arguments for SetStandAction. Must be four float values."
            )
            return

        try:
            stand_pitch = float(tokens[0])
            stand_roll = float(tokens[1])
            stand_yaw = float(tokens[2])
            stand_height = float(tokens[3])

            pitch_min, pitch_max = -0.1, 0.1
            roll_min, roll_max = -0.06, 0.06
            yaw_min, yaw_max = -0.25, 0.25
            height_min, height_max = -0.2, 0.0

            out_of_range = []
            if not (pitch_min <= stand_pitch <= pitch_max):
                out_of_range.append(f"Pitch ({stand_pitch})")
            if not (roll_min <= stand_roll <= roll_max):
                out_of_range.append(f"Roll ({stand_roll})")
            if not (yaw_min <= stand_yaw <= yaw_max):
                out_of_range.append(f"Yaw ({stand_yaw})")
            if not (height_min <= stand_height <= height_max):
                out_of_range.append(f"Height ({stand_height})")

            if out_of_range:
                print("Error: The following values are out of range:")
                for value in out_of_range:
                    print(f"  - {value}")
                print("Please enter values within the following ranges:")
                print("  - Pitch: [-0.1, 0.1]")
                print("  - Roll: [-0.06, 0.06]")
                print("  - Yaw: [-0.25, 0.25]")
                print("  - Base Height: [-0.2, 0.0]")
                return

            success, message = self.set_stand_action(
                stand_pitch, stand_roll, stand_yaw, stand_height
            )
            if success:
                print(f"Success: {message}")
            else:
                print(f"Failed: {message}")
        except ValueError:
            print(
                "Error: Invalid parameter for SetStandAction. Must be four float values."
            )
        self.lastcmd = None

    def set_mode(self, mode):
        request = adam_control_pb2.SetModeRequest(mode=mode)
        response = self.stub.SetMode(request)
        return response.success, response.message

    def set_stand_motion(self, motion):
        request = adam_control_pb2.SetStandMotionRequest(motion=motion)
        response = self.stub.SetStandMotion(request)
        return response.success, response.message

    def set_stand_carry_box(self, carry_box):
        request = adam_control_pb2.SetCarryBoxRequest(carry_box=carry_box)
        response = self.stub.SetStandCarryBox(request)
        return response.success, response.message

    def set_stand_action(self, stand_pitch, stand_roll, stand_yaw, stand_height):
        request = adam_control_pb2.SetActionRequest(
            stand_pitch=stand_pitch,
            stand_roll=stand_roll,
            stand_yaw=stand_yaw,
            stand_height=stand_height,
        )
        response = self.stub.SetStandAction(request)
        return response.success, response.message

    def set_stand_dynamic(self, dynamic_stand):
        request = adam_control_pb2.SetDynamicStandRequest(dynamic_stand=dynamic_stand)
        response = self.stub.SetStandDynamic(request)
        return response.success, response.message

    def set_speed(self, x_speed, y_speed, yaw_speed):
        request = adam_control_pb2.SetSpeedRequest(
            x_speed=x_speed, y_speed=y_speed, yaw_speed=yaw_speed
        )
        response = self.stub.SetSpeed(request)
        return response.success, response.message

    def auto_unigait_com(self, unigait_mode_com_x):
        request = adam_control_pb2.SetUnigaitCOMRequest(
            unigait_mode_com_x=unigait_mode_com_x
        )
        response = self.stub.AutoUnigaitCOM(request)
        return response.success, response.message

    def set_error_clear(self, error_clear_flag):
        request = adam_control_pb2.SetErrorClearRequest(
            error_clear_flag=error_clear_flag
        )
        response = self.stub.SetErrorClear(request)
        return response.success, response.message

    def set_control_mode(self, domain_id):
        """Queue a control paradigm switch: domain_id=0(Traditional), 1(RL).
        Returns immediately with (success, current_mode, message).
        current_mode reflects hardware (DDS rt/control_mode_state), not the queued target.
        Use wait_for_control_mode() to poll until hardware confirms.
        """
        request = adam_control_pb2.SetControlModeRequest(domain_id=domain_id)
        response = self.stub.SetControlMode(request)
        self.current_control_mode = response.current_mode
        return response.success, response.current_mode, response.message

    def wait_for_control_mode(self, target_domain_id, timeout_sec=CONTROL_MODE_POLL_TIMEOUT_SEC,
                              poll_interval=CONTROL_MODE_POLL_INTERVAL_SEC):
        """Poll GetControlState until hardware switches to target_domain_id or timeout.
        Prints live progress. Returns (success, final_mode, message).
        """
        target_str = control_mode_name(target_domain_id)
        print(f"  Waiting for hardware to switch to {target_str} "
              f"(timeout={timeout_sec}s, poll every {poll_interval}s)...")
        deadline = time.time() + timeout_sec
        last_printed_mode = None
        last_printed_detail = None
        elapsed = 0.0
        while time.time() < deadline:
            try:
                resp = self.stub.GetControlState(
                    adam_control_pb2.GetControlStateRequest()
                )
                cur = resp.domain_id
                cur_str = control_mode_name(cur)
                detail = resp.message or ""
                status_line = f"  [{elapsed:5.1f}s] Current mode: {cur} ({cur_str})"
                if detail and detail not in (cur_str, "Traditional", "RL"):
                    status_line += f" — {detail}"
                if cur != last_printed_mode or detail != last_printed_detail:
                    print(status_line)
                    last_printed_mode = cur
                    last_printed_detail = detail
                if cur == target_domain_id:
                    self.current_control_mode = cur
                    return True, cur, f"Switched to {target_str} successfully."
            except Exception as e:
                print(f"  GetControlState error: {e}")
            time.sleep(poll_interval)
            elapsed = time.time() - (deadline - timeout_sec)
        # 超时后再查一次
        try:
            resp = self.stub.GetControlState(adam_control_pb2.GetControlStateRequest())
            cur = resp.domain_id
            detail = resp.message or ""
        except Exception:
            cur = getattr(self, "current_control_mode", -1)
            detail = ""
        cur_str = control_mode_name(cur)
        extra = f" ({detail})" if detail else ""
        return False, cur, (
            f"Timeout ({timeout_sec}s): hardware still in mode={cur} ({cur_str}){extra}. "
            f"若底层 RL 未部署或 DDS 未连通，切换不会成功；可用 GetControlState 继续确认。"
        )

    def get_control_state(self):
        """Query current control mode state from DDS rt/control_mode_state.
        Prints the result; domain_id=-1 means state not yet received from hardware.
        """
        request = adam_control_pb2.GetControlStateRequest()
        response = self.stub.GetControlState(request)
        mode = response.domain_id
        mode_str = control_mode_name(mode)
        print(f"Control State: domain_id={mode} ({mode_str})")
        if response.message:
            print(f"  Message: {response.message}")
        if not response.success:
            print(f"  Warning: success=false")
        self.current_control_mode = mode

    def get_stand_list(self):
        request = adam_control_pb2.GetStandListRequest()
        response = self.stub.GetStandList(request)
        if response.success:
            print(f"Modes: {response.mode_list}")
            print(f"Motions: {response.motion_list}")
            print(f"Actions: {response.action_list}")
            print(f"Carry Boxes: {response.carrybox_list}")
            print(f"Balance Control: {response.balance_control}")
        else:
            print(f"Failed: {response.message}")

    def get_robot_state(self, print_msg):
        request = adam_control_pb2.GetRobotStateRequest()
        response = self.stub.GetRobotState(request)
        self.fsm_name = response.fsm_name
        self.current_motion = response.current_motion
        self.mode_enable_list = list(response.mode_enable_list)
        self.motion_enable_list = list(response.motion_enable_list)
        self.action_enable_list = list(response.action_enable_list)
        self.carrybox_enable_list = list(response.carrybox_enable_list)
        self.balance_control_enable = response.balance_control_enable
        self.stand_pitch = response.stand_pitch
        self.stand_roll = response.stand_roll
        self.stand_yaw = response.stand_yaw
        self.stand_height = response.stand_height
        self.x_vel = response.x_vel
        self.y_vel = response.y_vel
        self.yaw_vel = response.yaw_vel
        self.balance_control_state = response.balance_control_state
        self.message = response.message
        self.current_control_mode = response.current_control_mode

        self.supported_commands = [
            "SetMode",
            "GetStandList",
            "GetRobotState",
            "SetControlMode",
            "GetControlState",
            "exit",
            "help",
        ]

        if self.fsm_name == "Stand":
            self.supported_commands.extend(
                [
                    "SetStandMotion",
                    "SetStandCarryBox",
                    "SetStandAction",
                    "SetStandDynamic",
                ]
            )

        if self.fsm_name in ["Walk", "Run"]:
            self.supported_commands.extend(
                [
                    "SetSpeed",
                    "AutoUnigaitCOM",
                ]
            )

        if self.fsm_name == "Stop":
            self.supported_commands.append("SetErrorClear")

        if print_msg:
            print(f"Current Mode: {response.fsm_name}")
            print(f"Current Motion: {response.current_motion}")
            print(f"Enable Mode List: {response.mode_enable_list}")
            print(f"Enable Motion List: {response.motion_enable_list}")
            print(f"Enable Action List: {response.action_enable_list}")
            print(f"Enable Carry Box List: {response.carrybox_enable_list}")
            print(f"Stand Pitch: {response.stand_pitch}")
            print(f"Stand Roll: {response.stand_roll}")
            print(f"Stand Yaw: {response.stand_yaw}")
            print(f"Stand Height: {response.stand_height}")
            print(f"X Velocity: {response.x_vel}")
            print(f"Y Velocity: {response.y_vel}")
            print(f"Yaw Velocity: {response.yaw_vel}")
            print(f"Balance Control State: {response.balance_control_state}")
            ctrl_mode = response.current_control_mode
            print(f"Control Mode: {ctrl_mode} ({control_mode_name(ctrl_mode)})")

    def complete(self, text, state):
        """Override the complete method to handle custom completion logic."""
        self.get_robot_state(False)
        if self.state == "WAIT_COMMAND":
            normalized_text = text.upper()
            matches = [
                cmd
                for cmd in self.supported_commands
                if cmd.upper().startswith(normalized_text)
            ]
            if state < len(matches):
                return matches[state]
            else:
                return None
        elif self.state == "WAIT_PARAMETER":
            normalized_text = text.upper()
            if self.pending_command in ["SETSTANDDYNAMIC", "AUTOUNIGAITCOM"]:
                matches = [
                    param
                    for param in ["true", "false"]
                    if param.startswith(normalized_text.lower())
                ]
            else:
                matches = [
                    param
                    for param in self.enable_list
                    if param.upper().startswith(normalized_text)
                ]
            if state < len(matches):
                return matches[state]
            else:
                return None
        else:
            return None


def _do_set_control_mode(client, domain_id):
    """Shared logic for SetControlMode used by both handle_parameter and execute_command.

    Flow:
      1. Call set_control_mode() to queue the DDS command (returns immediately).
      2. If server says 'already at target': done.
      3. For parameter-validation errors: print and return.
      4. Otherwise poll wait_for_control_mode() until hardware confirms.
    Returns (success, message).
    """
    target_str = "Traditional" if domain_id == 0 else "RL"
    ok, cur_mode, msg = client.set_control_mode(domain_id)

    # Parameter validation failure (invalid domain_id, etc.)
    if not ok and ("Invalid" in msg or "invalid" in msg):
        print(f"Failed: {msg}")
        return False, msg

    if cur_mode == domain_id and ("Already" in msg or "already" in msg):
        print(f"Already in {target_str} mode.")
        return True, f"Already in {target_str} mode."

    # 命令已入队，打印服务端反馈并轮询 hardware
    if ok:
        print(f"  {msg}")
    else:
        print(f"  Note: {msg}")
        print(f"  Polling hardware state (DDS command was sent)...")

    poll_ok, final_mode, poll_msg = client.wait_for_control_mode(domain_id)
    if poll_ok:
        print(f"Success: {poll_msg}")
    else:
        print(f"Failed: {poll_msg}")
    return poll_ok, poll_msg


def execute_command(client, tokens):
    command = tokens[
        0
    ].upper()  # Convert command to uppercase for case-insensitive matching
    message = ""
    success = False

    try:
        if command == "SETMODE":
            if len(tokens) != 2:
                raise ValueError("Error: Invalid number of arguments for SetMode.")
            mode = tokens[1]
            if mode not in client.enable_list:  # Check if mode is valid
                raise ValueError(
                    "Error: Invalid mode. Please enter a valid option from the enable list."
                )
            success, message = client.set_mode(mode)
        elif command == "SETSTANDMOTION":
            if len(tokens) != 2:
                raise ValueError(
                    "Error: Invalid number of arguments for SetStandMotion."
                )
            motion = tokens[1]
            if motion not in client.enable_list:  # Check if motion is valid
                raise ValueError(
                    "Error: Invalid motion. Please enter a valid option from the enable list."
                )
            success, message = client.set_stand_motion(motion)
        elif command == "SETSTANDCARRYBOX":
            if len(tokens) != 2:
                raise ValueError(
                    "Error: Invalid number of arguments for SetStandCarryBox."
                )
            carry_box = tokens[1]
            if carry_box not in client.enable_list:  # Check if carry_box is valid
                raise ValueError(
                    "Error: Invalid carry box. Please enter a valid option from the enable list."
                )
            success, message = client.set_stand_carry_box(carry_box)
        elif command == "SETSTANDACTION":
            if len(tokens) != 5:
                raise ValueError(
                    "Error: Invalid number of arguments for SetStandAction."
                )
            stand_pitch = float(tokens[1])
            stand_roll = float(tokens[2])
            stand_yaw = float(tokens[3])
            stand_height = float(tokens[4])
            success, message = client.set_stand_action(
                stand_pitch, stand_roll, stand_yaw, stand_height
            )
        elif command == "SETSTANDDYNAMIC":
            if len(tokens) != 2:
                raise ValueError(
                    "Error: Invalid number of arguments for SetStandDynamic."
                )
            dynamic_stand_str = tokens[1]
            if dynamic_stand_str.lower() not in [
                "true",
                "false",
            ]:  # Check if parameter is valid
                raise ValueError(
                    "Error: Invalid parameter for SetStandDynamic. Must be 'true' or 'false'."
                )
            dynamic_stand = dynamic_stand_str.lower() == "true"
            success, message = client.set_stand_dynamic(dynamic_stand)
        elif command == "SETSPEED":
            if len(tokens) != 4:
                raise ValueError("Error: Invalid number of arguments for SetSpeed.")
            x_speed = float(tokens[1])
            y_speed = float(tokens[2])
            yaw_speed = float(tokens[3])
            success, message = client.set_speed(x_speed, y_speed, yaw_speed)
        elif command == "AUTOUNIGAITCOM":
            if len(tokens) != 2:
                raise ValueError(
                    "Error: Invalid number of arguments for AutoUnigaitCOM."
                )
            unigait_mode_com_x_str = tokens[1]
            if unigait_mode_com_x_str.lower() not in [
                "true",
                "false",
            ]:  # Check if parameter is valid
                raise ValueError(
                    "Error: Invalid parameter for AutoUnigaitCOM. Must be 'true' or 'false'."
                )
            unigait_mode_com_x = unigait_mode_com_x_str.lower() == "true"
            success, message = client.auto_unigait_com(unigait_mode_com_x)
        elif command == "SETCONTROLMODE":
            if len(tokens) != 2:
                raise ValueError(
                    "Error: Invalid number of arguments for SetControlMode. Usage: SetControlMode <0|1>"
                )
            try:
                domain_id = int(tokens[1])
            except ValueError:
                raise ValueError("Error: domain_id must be integer 0(Traditional) or 1(RL).")
            if domain_id not in [0, 1]:
                raise ValueError("Error: domain_id must be 0(Traditional) or 1(RL).")
            success, message = _do_set_control_mode(client, domain_id)
        elif command == "GETCONTROLSTATE":
            client.get_control_state()
            success = True
            message = ""
        else:
            raise ValueError(f"Unknown command: {command}")
    except ValueError as e:
        message = str(e)

    return success, message


if __name__ == "__main__":
    try:
        config_path = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "..", "ip_config.json"
        )
        with open(config_path, "r", encoding="utf-8") as file:
            config = json.load(file)

        server_ip = config["server"]["ip"]
        if not server_ip or server_ip in ("xx.xx.xx.xx", "0.0.0.0"):
            raise ValueError(
                f"请在 {config_path} 中配置机器人实际 IP 地址（当前: {server_ip!r}）"
            )

        server_port = config.get("server", {}).get("port", 6666)
        server_address = f"{server_ip}:{server_port}"

        client = AdamCommand(server_address)
        client.cmdloop()
    except (ConnectionError, ValueError) as e:
        print(f"连接失败: {e}")
    except grpc.RpcError as e:
        print(f"gRPC 调用失败: {e.code().name} - {e.details()}")
    except Exception as e:
        print(f"错误: {e}")
