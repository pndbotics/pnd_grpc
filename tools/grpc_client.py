#!/usr/bin/env python3
"""PND Robot gRPC 命令行客户端。

功能：
- Tab 补全所有可用命令和参数
- 上下键历史记录（readline）
- 状态感知：根据当前 FSM 状态动态显示可用操作

Usage:
    python3 tools/grpc_client.py [--addr HOST:PORT]
"""

from __future__ import annotations

import cmd
import os
import readline
import sys
import time
from pathlib import Path

_REPO_ROOT = str(Path(__file__).resolve().parent.parent)
if _REPO_ROOT not in sys.path:
    sys.path.insert(0, _REPO_ROOT)

import grpc

from comm.grpc import robot_control_pb2 as pb2
from comm.grpc import robot_control_pb2_grpc as pb2_grpc

_HISTORY_FILE = os.path.expanduser("~/.pnd_grpc_client_history")
_HISTORY_LEN = 500


class RobotCommandClient(cmd.Cmd):
    intro = (
        "╔══════════════════════════════════════════╗\n"
        "║   PND Robot Control Client v1.0          ║\n"
        "║   Type 'help' for commands, Tab to complete ║\n"
        "╚══════════════════════════════════════════╝\n"
    )
    prompt = "robot> "

    def __init__(self, addr: str):
        super().__init__()
        self._addr = addr
        self._channel = grpc.insecure_channel(addr)
        self._stub = pb2_grpc.RobotControlStub(self._channel)
        self._state: dict = {}
        self._load_history()
        self._refresh_state(silent=True)
        if self._state:
            print(f"  Connected: {addr}  |  FSM: {self._state.get('fsm_state', '?')}")
        else:
            print(f"  Connected: {addr}  |  (unable to query state)")
        print()

    def _load_history(self):
        try:
            if os.path.exists(_HISTORY_FILE):
                readline.read_history_file(_HISTORY_FILE)
        except Exception:
            pass
        readline.set_history_length(_HISTORY_LEN)

    def _save_history(self):
        try:
            readline.write_history_file(_HISTORY_FILE)
        except Exception:
            pass

    def _refresh_state(self, silent: bool = False):
        try:
            resp = self._stub.GetRobotState(pb2.GetRobotStateRequest())
            self._state = {
                "fsm_state": resp.fsm_state,
                "vx": resp.vx,
                "vy": resp.vy,
                "vyaw": resp.vyaw,
                "height": resp.height,
                "current_motion_file": resp.current_motion_file,
                "motion_playing": resp.motion_playing,
                "current_tracking_motion": resp.current_tracking_motion,
                "tracking_playing": resp.tracking_playing,
                "switchable_states": list(resp.switchable_states),
                "available_actions": list(resp.available_actions),
            }
        except grpc.RpcError as e:
            if not silent:
                print(f"  [ERROR] RPC failed: {e.code().name} - {e.details()}")
            self._state = {}

    def _wait_motion_state(
        self,
        expected_playing: bool,
        expected_file: str | None = None,
        timeout_sec: float = 1.0,
    ) -> None:
        deadline = time.monotonic() + timeout_sec
        while time.monotonic() < deadline:
            self._refresh_state(silent=True)
            if not self._state:
                time.sleep(0.05)
                continue
            playing_ok = self._state.get("motion_playing") == expected_playing
            file_ok = (
                expected_file is None
                or self._state.get("current_motion_file") == expected_file
            )
            if playing_ok and file_ok:
                break
            time.sleep(0.05)

        motion_file = self._state.get("current_motion_file") or "(none)"
        motion_playing = self._state.get("motion_playing", False)
        print(
            f"  Observed: motion_file={motion_file}, "
            f"playing={motion_playing}"
        )

    # ------------------------------------------------------------------
    # Commands
    # ------------------------------------------------------------------

    def do_state(self, arg):
        """Query and display current robot state."""
        self._refresh_state()
        if not self._state:
            return
        s = self._state
        print(f"  FSM State:         {s['fsm_state']}")
        print(f"  Velocity:          vx={s['vx']:.3f}  vy={s['vy']:.3f}  vyaw={s['vyaw']:.3f}")
        print(f"  Height:            {s['height']:.3f}")
        print(f"  Motion File:       {s['current_motion_file'] or '(none)'}")
        print(f"  Motion Playing:    {s['motion_playing']}")
        print(f"  Tracking Motion:   {s['current_tracking_motion'] or '(none)'}")
        print(f"  Tracking Playing:  {s['tracking_playing']}")
        print(f"  Switchable:        {', '.join(s['switchable_states']) or '(none)'}")
        print(f"  Available Actions: {', '.join(s['available_actions']) or '(none)'}")

    def do_mode(self, arg):
        """Switch FSM mode. Usage: mode <STATE_NAME>"""
        if not arg:
            self._refresh_state()
            print(f"  Current: {self._state.get('fsm_state', '?')}")
            print(f"  Switchable: {', '.join(self._state.get('switchable_states', []))}")
            print("  Usage: mode <STATE_NAME>")
            return
        try:
            resp = self._stub.SetMode(pb2.SetModeRequest(target_state=arg.strip()))
            status = "OK" if resp.success else "FAIL"
            print(f"  [{status}] {resp.message}  (current={resp.current_state})")
        except grpc.RpcError as e:
            print(f"  [ERROR] {e.code().name}: {e.details()}")

    def complete_mode(self, text, line, begidx, endidx):
        self._refresh_state(silent=True)
        candidates = self._state.get("switchable_states", [])
        return [s for s in candidates if s.upper().startswith(text.upper())]

    def do_controlmode(self, arg):
        """Set control paradigm via DDS. Usage: controlmode <0|1>  (0=Traditional, 1=RL)"""
        if not arg.strip():
            print("  Usage: controlmode <0|1>")
            return
        try:
            domain_id = int(arg.strip(), 10)
        except ValueError:
            print(f"  Invalid domain_id: {arg!r}")
            return
        try:
            resp = self._stub.SetControlMode(pb2.SetControlModeRequest(domain_id=domain_id))
            status = "OK" if resp.success else "FAIL"
            print(f"  [{status}] {resp.message}")
        except grpc.RpcError as e:
            print(f"  [ERROR] {e.code().name}: {e.details()}")

    def complete_controlmode(self, text, line, begidx, endidx):
        return [v for v in ("0", "1") if v.startswith(text)]

    def do_controlstate(self, arg):
        """Query control paradigm from DDS state. Usage: controlstate"""
        try:
            resp = self._stub.GetControlState(pb2.GetControlStateRequest())
            status = "OK" if resp.success else "FAIL"
            print(f"  [{status}] {resp.message}  domain_id={resp.domain_id}")
        except grpc.RpcError as e:
            print(f"  [ERROR] {e.code().name}: {e.details()}")

    def do_motion(self, arg):
        """Control motion playback. Usage: motion play <path> | motion stop"""
        parts = arg.strip().split(maxsplit=1)
        if not parts:
            print("  Usage: motion play <file_path>")
            print("         motion stop")
            return

        subcmd = parts[0].lower()
        if subcmd == "play":
            if len(parts) < 2:
                print("  Usage: motion play <file_path>")
                return
            file_path = parts[1].strip()
            try:
                resp = self._stub.SetMotion(pb2.SetMotionRequest(
                    command=pb2.SetMotionRequest.PLAY, motion_file=file_path
                ))
                status = "OK" if resp.success else "FAIL"
                print(
                    f"  [{status}] {resp.message}  "
                    f"(file={resp.current_motion}, playing={resp.is_playing})"
                )
                if resp.success:
                    self._wait_motion_state(
                        expected_playing=True,
                        expected_file=resp.current_motion,
                    )
            except grpc.RpcError as e:
                print(f"  [ERROR] {e.code().name}: {e.details()}")
        elif subcmd == "stop":
            if len(parts) > 1:
                print("  Note: motion stop ignores extra arguments.")
            try:
                resp = self._stub.SetMotion(pb2.SetMotionRequest(
                    command=pb2.SetMotionRequest.STOP
                ))
                status = "OK" if resp.success else "FAIL"
                print(f"  [{status}] {resp.message}")
                if resp.success:
                    self._wait_motion_state(expected_playing=False)
            except grpc.RpcError as e:
                print(f"  [ERROR] {e.code().name}: {e.details()}")
        else:
            print(f"  Unknown sub-command: {subcmd}")
            print("  Usage: motion play <path> | motion stop")

    def complete_motion(self, text, line, begidx, endidx):
        parts = line.split()
        if len(parts) == 1 or (len(parts) == 2 and not line.endswith(" ")):
            subcmds = ["play", "stop"]
            return [s for s in subcmds if s.startswith(text.lower())]
        if len(parts) >= 2 and parts[1].lower() == "play":
            return self._complete_path(text)
        return []

    def _complete_path(self, text: str) -> list[str]:
        """补全文件/目录路径。"""
        import glob as _glob
        if not text:
            text = "./"
        if os.path.isdir(text) and not text.endswith("/"):
            text += "/"
        matches = _glob.glob(text + "*")
        results = []
        for m in matches[:20]:
            if os.path.isdir(m):
                results.append(m + "/")
            else:
                results.append(m)
        return results

    def do_tracking(self, arg):
        """Switch tracking motion file. Usage: tracking <file_path>"""
        if not arg:
            print("  Usage: tracking <file_path>")
            return
        try:
            resp = self._stub.SetTrackingMotion(pb2.SetTrackingMotionRequest(
                motion_file=arg.strip()
            ))
            status = "OK" if resp.success else "FAIL"
            print(f"  [{status}] {resp.message}  (current={resp.current_tracking_motion})")
        except grpc.RpcError as e:
            print(f"  [ERROR] {e.code().name}: {e.details()}")

    def complete_tracking(self, text, line, begidx, endidx):
        return self._complete_path(text)

    def do_velocity(self, arg):
        """Set walking velocity (reserved). Usage: velocity <vx> <vy> <vyaw>"""
        parts = arg.strip().split()
        if len(parts) == 0 or (len(parts) == 1 and not parts[0]):
            self._refresh_state()
            s = self._state
            print(f"  Current: vx={s.get('vx', 0):.3f}  vy={s.get('vy', 0):.3f}  vyaw={s.get('vyaw', 0):.3f}")
            print("  Usage: velocity <vx> <vy> <vyaw>  (values in [-1, 1])")
            return
        if len(parts) != 3:
            print("  Usage: velocity <vx> <vy> <vyaw>  (values in [-1, 1])")
            return
        try:
            vx, vy, vyaw = float(parts[0]), float(parts[1]), float(parts[2])
            resp = self._stub.SetVelocity(pb2.SetVelocityRequest(vx=vx, vy=vy, vyaw=vyaw))
            status = "OK" if resp.success else "FAIL"
            print(f"  [{status}] {resp.message}")
        except ValueError:
            print("  [ERROR] All values must be floats")
        except grpc.RpcError as e:
            print(f"  [ERROR] {e.code().name}: {e.details()}")

    def do_height(self, arg):
        """Set standing height (reserved). Usage: height <value>"""
        if not arg:
            self._refresh_state()
            print(f"  Current: {self._state.get('height', 0):.3f}")
            print("  Usage: height <value>  (value in [-1, 1])")
            return
        try:
            h = float(arg.strip())
            resp = self._stub.SetHeight(pb2.SetHeightRequest(height=h))
            status = "OK" if resp.success else "FAIL"
            print(f"  [{status}] {resp.message}")
        except ValueError:
            print("  [ERROR] Value must be a float")
        except grpc.RpcError as e:
            print(f"  [ERROR] {e.code().name}: {e.details()}")

    def do_shutdown(self, arg):
        """Send shutdown request to the controller."""
        try:
            resp = self._stub.Shutdown(pb2.ShutdownRequest(force=bool(arg.strip())))
            status = "OK" if resp.success else "FAIL"
            print(f"  [{status}] {resp.message}")
        except grpc.RpcError as e:
            print(f"  [ERROR] {e.code().name}: {e.details()}")

    def do_clear(self, arg):
        """Clear terminal screen."""
        os.system("clear" if os.name != "nt" else "cls")

    def do_quit(self, arg):
        """Exit the client."""
        print("  Bye.")
        return True

    do_exit = do_quit
    do_EOF = do_quit

    # ------------------------------------------------------------------
    # Overrides
    # ------------------------------------------------------------------

    def default(self, line):
        print(f"  Unknown command: '{line}'. Type 'help' for available commands.")

    def emptyline(self):
        pass

    def get_names(self):
        return dir(self)

    def completenames(self, text, *ignored):
        self._refresh_state(silent=True)
        actions = set(self._state.get("available_actions", []))
        commands = ["state", "mode", "clear", "quit", "exit", "help"]
        if "SetMotion" in actions:
            commands.append("motion")
        if "SetTrackingMotion" in actions:
            commands.append("tracking")
        if "SetVelocity" in actions:
            commands.append("velocity")
        if "SetHeight" in actions:
            commands.append("height")
        commands.append("shutdown")
        return [c for c in commands if c.startswith(text.lower())]

    def postcmd(self, stop, line):
        self._save_history()
        return stop

    def do_help(self, arg):
        """Show available commands."""
        if arg:
            super().do_help(arg)
            return
        self._refresh_state(silent=True)
        actions = set(self._state.get("available_actions", []))
        fsm = self._state.get("fsm_state", "?")

        print(f"  Current FSM: {fsm}")
        print()
        print("  Global commands:")
        print("    state              Query current robot state")
        print("    mode [STATE]       Switch FSM mode (Tab for options)")
        print("    controlmode <0|1>  Set control paradigm (0=Traditional, 1=RL)")
        print("    controlstate       Query control paradigm from DDS state")
        print("    shutdown           Shutdown controller")
        print("    clear              Clear screen")
        print("    quit / exit        Exit client")
        if actions:
            print()
            print(f"  Available in {fsm}:")
            if "SetMotion" in actions:
                print("    motion play|stop   Control motion playback")
            if "SetTrackingMotion" in actions:
                print("    tracking <path>    Switch tracking motion file")
            if "SetVelocity" in actions:
                print("    velocity vx vy vyaw  Set velocity (reserved)")
            if "SetHeight" in actions:
                print("    height <value>     Set height (reserved)")


def main():
    import argparse
    parser = argparse.ArgumentParser(description="PND Robot gRPC Client")
    parser.add_argument("--addr", default="localhost:50051", help="Server address (host:port)")
    args = parser.parse_args()

    try:
        client = RobotCommandClient(args.addr)
        client.cmdloop()
    except KeyboardInterrupt:
        print("\n  Interrupted.")
    except Exception as e:
        print(f"  [FATAL] {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
