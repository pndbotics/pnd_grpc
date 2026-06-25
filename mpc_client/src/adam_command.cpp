#include "../include/adam_command.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace adam_control {

std::string ControlModeName(int domain_id) {
  switch (domain_id) {
    case 0:
      return "Traditional";
    case 1:
      return "RL";
    case -1:
      return "Unknown (not yet received)";
    default:
      return "Unknown (" + std::to_string(domain_id) + ")";
  }
}

AdamCommand::AdamCommand(const std::string& server_address) {
  grpc::ChannelArguments args;
  args.SetInt("grpc.enable_http_proxy", 0);
  args.SetInt("grpc.keepalive_time_ms", 10000);
  args.SetInt("grpc.keepalive_timeout_ms", 5000);

  channel_ = grpc::CreateCustomChannel(server_address, grpc::InsecureChannelCredentials(), args);
  const auto deadline =
      std::chrono::system_clock::now() + std::chrono::seconds(kGrpcConnectTimeoutSec);
  if (!channel_->WaitForConnected(deadline)) {
    throw std::runtime_error(
        "无法连接到 gRPC 服务 " + server_address +
        "。请确认机器人端 PndControl 已启动（grpc_on=TRUE）、ip_config.json IP 正确且端口 6666 可达。");
  }

  stub_ = adam_control::RobotControl::NewStub(channel_);
}

bool AdamCommand::SetMode(const std::string& mode, std::string& message) {
  grpc::ClientContext context;
  adam_control::SetModeRequest request;
  request.set_mode(mode);
  adam_control::SetModeResponse response;

  grpc::Status status = stub_->SetMode(&context, request, &response);

  if (status.ok()) {
    message = response.message();
    return response.success();
  }
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::SetStandMotion(const std::string& motion, std::string& message) {
  grpc::ClientContext context;
  adam_control::SetStandMotionRequest request;
  request.set_motion(motion);
  adam_control::SetStandMotionResponse response;

  grpc::Status status = stub_->SetStandMotion(&context, request, &response);

  if (status.ok()) {
    message = response.message();
    return response.success();
  }
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::SetStandCarryBox(const std::string& carry_box, std::string& message) {
  grpc::ClientContext context;
  adam_control::SetCarryBoxRequest request;
  request.set_carry_box(carry_box);
  adam_control::SetCarryBoxResponse response;

  grpc::Status status = stub_->SetStandCarryBox(&context, request, &response);

  if (status.ok()) {
    message = response.message();
    return response.success();
  }
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::SetStandAction(double stand_pitch, double stand_roll, double stand_yaw, double stand_height,
                                 std::string& message) {
  grpc::ClientContext context;
  adam_control::SetActionRequest request;
  request.set_stand_pitch(stand_pitch);
  request.set_stand_roll(stand_roll);
  request.set_stand_yaw(stand_yaw);
  request.set_stand_height(stand_height);
  adam_control::SetActionResponse response;

  grpc::Status status = stub_->SetStandAction(&context, request, &response);

  if (status.ok()) {
    message = response.message();
    return response.success();
  }
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::SetStandDynamic(bool dynamic_stand, std::string& message) {
  grpc::ClientContext context;
  adam_control::SetDynamicStandRequest request;
  request.set_dynamic_stand(dynamic_stand);
  adam_control::SetDynamicStandResponse response;

  grpc::Status status = stub_->SetStandDynamic(&context, request, &response);

  if (status.ok()) {
    message = response.message();
    return response.success();
  }
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::SetSpeed(double x_speed, double y_speed, double yaw_speed, std::string& message) {
  grpc::ClientContext context;
  adam_control::SetSpeedRequest request;
  request.set_x_speed(x_speed);
  request.set_y_speed(y_speed);
  request.set_yaw_speed(yaw_speed);
  adam_control::SetSpeedResponse response;

  grpc::Status status = stub_->SetSpeed(&context, request, &response);

  if (status.ok()) {
    message = response.message();
    return response.success();
  }
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::AutoUnigaitCOM(bool unigait_mode_com_x, std::string& message) {
  grpc::ClientContext context;
  adam_control::SetUnigaitCOMRequest request;
  request.set_unigait_mode_com_x(unigait_mode_com_x);
  adam_control::SetUnigaitCOMResponse response;

  grpc::Status status = stub_->AutoUnigaitCOM(&context, request, &response);

  if (status.ok()) {
    message = response.message();
    return response.success();
  }
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::SetErrorClear(bool error_clear_flag, std::string& message) {
  grpc::ClientContext context;
  adam_control::SetErrorClearRequest request;
  request.set_error_clear_flag(error_clear_flag);
  adam_control::SetErrorClearResponse response;

  grpc::Status status = stub_->SetErrorClear(&context, request, &response);

  if (status.ok()) {
    message = response.message();
    return response.success();
  }
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::GetStandList(bool& stand_list_flag, std::vector<std::string>& mode_list,
                               std::vector<std::string>& motion_list, std::vector<std::string>& action_list,
                               std::vector<std::string>& carrybox_list, std::string& balance_control,
                               std::string& message) {
  grpc::ClientContext context;
  adam_control::GetStandListRequest request;
  request.set_mode_list_req(stand_list_flag);
  adam_control::GetStandListResponse response;

  grpc::Status status = stub_->GetStandList(&context, request, &response);

  if (status.ok()) {
    mode_list.assign(response.mode_list().begin(), response.mode_list().end());
    motion_list.assign(response.motion_list().begin(), response.motion_list().end());
    action_list.assign(response.action_list().begin(), response.action_list().end());
    carrybox_list.assign(response.carrybox_list().begin(), response.carrybox_list().end());
    balance_control = response.balance_control();
    message = response.message();
    return response.success();
  }
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::SetControlMode(int domain_id, int& confirmed_mode, std::string& message) {
  grpc::ClientContext context;
  adam_control::SetControlModeRequest request;
  request.set_domain_id(domain_id);
  adam_control::SetControlModeResponse response;

  grpc::Status status = stub_->SetControlMode(&context, request, &response);

  if (status.ok()) {
    confirmed_mode = response.current_mode();
    message = response.message();
    return response.success();
  }
  confirmed_mode = -1;
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::GetControlState(int& domain_id, std::string& message) {
  grpc::ClientContext context;
  adam_control::GetControlStateRequest request;
  adam_control::GetControlStateResponse response;

  grpc::Status status = stub_->GetControlState(&context, request, &response);

  if (status.ok()) {
    domain_id = response.domain_id();
    message = response.message();
    return response.success();
  }
  domain_id = -1;
  message = "RPC failed: " + status.error_message();
  return false;
}

bool AdamCommand::WaitForControlMode(int target_domain_id, int& final_mode, std::string& message,
                                     double timeout_sec, double poll_interval_sec) {
  const std::string target_str = ControlModeName(target_domain_id);
  std::cout << "  Waiting for hardware to switch to " << target_str << " (timeout=" << timeout_sec
            << "s, poll every " << poll_interval_sec << "s)..." << std::endl;

  const auto start = std::chrono::steady_clock::now();
  int last_printed_mode = -2;
  while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < timeout_sec) {
    int domain_id = -1;
    std::string poll_message;
    if (GetControlState(domain_id, poll_message)) {
      if (domain_id != last_printed_mode) {
        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "  [" << elapsed << "s] Current mode: " << domain_id << " ("
                  << ControlModeName(domain_id) << ")" << std::endl;
        last_printed_mode = domain_id;
      }
      if (domain_id == target_domain_id) {
        final_mode = domain_id;
        message = "Switched to " + target_str + " successfully.";
        return true;
      }
    } else {
      std::cout << "  GetControlState error: " << poll_message << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(poll_interval_sec));
  }

  std::string poll_message;
  if (!GetControlState(final_mode, poll_message)) {
    final_mode = -1;
  }
  message = "Timeout (" + std::to_string(static_cast<int>(timeout_sec)) +
            "s): hardware still in mode=" + std::to_string(final_mode) + " (" +
            ControlModeName(final_mode) +
            "). Switch may still be in progress — use GetControlState to verify.";
  return false;
}

bool AdamCommand::SetControlModeAndWait(int domain_id, int& final_mode, std::string& message) {
  const std::string target_str = ControlModeName(domain_id);
  int confirmed_mode = -1;
  const bool ok = SetControlMode(domain_id, confirmed_mode, message);

  if (!ok && (message.find("Invalid") != std::string::npos || message.find("invalid") != std::string::npos)) {
    return false;
  }

  if (confirmed_mode == domain_id) {
    final_mode = confirmed_mode;
    message = "Already in " + target_str + " mode.";
    return true;
  }

  if (!ok) {
    std::cout << "  Note: " << message << std::endl;
    std::cout << "  Polling hardware state (DDS command was sent)..." << std::endl;
  }

  return WaitForControlMode(domain_id, final_mode, message);
}

bool AdamCommand::GetRobotState(bool& robot_state_flag, std::string& fsm_name, std::string& current_motion,
                                std::vector<std::string>& mode_enable_list,
                                std::vector<std::string>& motion_enable_list,
                                std::vector<std::string>& action_enable_list,
                                std::vector<std::string>& carrybox_enable_list, std::string& balance_control_enable,
                                double& stand_pitch, double& stand_roll, double& stand_yaw, double& stand_height,
                                double& x_vel, double& y_vel, double& yaw_vel, bool& balance_control_state,
                                bool& motion_files_enable, int& current_control_mode, std::string& message) {
  grpc::ClientContext context;
  adam_control::GetRobotStateRequest request;
  request.set_get_state_flag(robot_state_flag);
  adam_control::GetRobotStateResponse response;

  grpc::Status status = stub_->GetRobotState(&context, request, &response);
  if (status.ok()) {
    fsm_name = response.fsm_name();
    current_motion = response.current_motion();
    mode_enable_list.assign(response.mode_enable_list().begin(), response.mode_enable_list().end());
    motion_enable_list.assign(response.motion_enable_list().begin(), response.motion_enable_list().end());
    action_enable_list.assign(response.action_enable_list().begin(), response.action_enable_list().end());
    carrybox_enable_list.assign(response.carrybox_enable_list().begin(), response.carrybox_enable_list().end());
    balance_control_enable = response.balance_control_enable();
    stand_pitch = response.stand_pitch();
    stand_roll = response.stand_roll();
    stand_yaw = response.stand_yaw();
    stand_height = response.stand_height();
    x_vel = response.x_vel();
    y_vel = response.y_vel();
    yaw_vel = response.yaw_vel();
    balance_control_state = response.balance_control_state();
    motion_files_enable = response.motion_files_enable();
    current_control_mode = response.current_control_mode();
    message = response.message();
    return response.success();
  }
  message = "RPC failed: " + status.error_message();
  return false;
}

void AdamCommand::SetControlModeAsync(
    int domain_id, std::function<void(bool success, int confirmed_mode, const std::string& message)> callback) {
  grpc::ClientContext context;
  adam_control::SetControlModeRequest request;
  request.set_domain_id(domain_id);
  adam_control::SetControlModeResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::SetControlModeResponse>> rpc(
      stub_->AsyncSetControlMode(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)20);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    callback(response.success(), response.current_mode(), response.message());
  } else {
    callback(false, -1, "RPC failed: " + status_.error_message());
  }
}

void AdamCommand::SetModeAsync(const std::string& mode,
                               std::function<void(bool success, const std::string& message)> callback) {
  grpc::ClientContext context;
  adam_control::SetModeRequest request;
  request.set_mode(mode);
  adam_control::SetModeResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::SetModeResponse>> rpc(
      stub_->AsyncSetMode(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)1);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    callback(response.success(), response.message());
  } else {
    callback(false, "RPC failed: " + status_.error_message());
  }
}

void AdamCommand::SetStandMotionAsync(const std::string& motion,
                                      std::function<void(bool success, const std::string& message)> callback) {
  grpc::ClientContext context;
  adam_control::SetStandMotionRequest request;
  request.set_motion(motion);
  adam_control::SetStandMotionResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::SetStandMotionResponse>> rpc(
      stub_->AsyncSetStandMotion(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)7);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    callback(response.success(), response.message());
  } else {
    callback(false, "RPC failed: " + status_.error_message());
  }
}

void AdamCommand::SetStandCarryBoxAsync(const std::string& carry_box,
                                        std::function<void(bool success, const std::string& message)> callback) {
  grpc::ClientContext context;
  adam_control::SetCarryBoxRequest request;
  request.set_carry_box(carry_box);
  adam_control::SetCarryBoxResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::SetCarryBoxResponse>> rpc(
      stub_->AsyncSetStandCarryBox(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)8);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    callback(response.success(), response.message());
  } else {
    callback(false, "RPC failed: " + status_.error_message());
  }
}

void AdamCommand::SetStandActionAsync(double stand_pitch, double stand_roll, double stand_yaw, double stand_height,
                                      std::function<void(bool success, const std::string& message)> callback) {
  grpc::ClientContext context;
  adam_control::SetActionRequest request;
  request.set_stand_pitch(stand_pitch);
  request.set_stand_roll(stand_roll);
  request.set_stand_yaw(stand_yaw);
  request.set_stand_height(stand_height);
  adam_control::SetActionResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::SetActionResponse>> rpc(
      stub_->AsyncSetStandAction(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)9);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    callback(response.success(), response.message());
  } else {
    callback(false, "RPC failed: " + status_.error_message());
  }
}

void AdamCommand::SetStandDynamicAsync(bool dynamic_stand,
                                       std::function<void(bool success, const std::string& message)> callback) {
  grpc::ClientContext context;
  adam_control::SetDynamicStandRequest request;
  request.set_dynamic_stand(dynamic_stand);
  adam_control::SetDynamicStandResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::SetDynamicStandResponse>> rpc(
      stub_->AsyncSetStandDynamic(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)10);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    callback(response.success(), response.message());
  } else {
    callback(false, "RPC failed: " + status_.error_message());
  }
}

void AdamCommand::SetSpeedAsync(double x_speed, double y_speed, double yaw_speed,
                                std::function<void(bool success, const std::string& message)> callback) {
  grpc::ClientContext context;
  adam_control::SetSpeedRequest request;
  request.set_x_speed(x_speed);
  request.set_y_speed(y_speed);
  request.set_yaw_speed(yaw_speed);
  adam_control::SetSpeedResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::SetSpeedResponse>> rpc(
      stub_->AsyncSetSpeed(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)12);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    callback(response.success(), response.message());
  } else {
    callback(false, "RPC failed: " + status_.error_message());
  }
}

void AdamCommand::AutoUnigaitCOMAsync(bool unigait_mode_com_x,
                                      std::function<void(bool success, const std::string& message)> callback) {
  grpc::ClientContext context;
  adam_control::SetUnigaitCOMRequest request;
  request.set_unigait_mode_com_x(unigait_mode_com_x);
  adam_control::SetUnigaitCOMResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::SetUnigaitCOMResponse>> rpc(
      stub_->AsyncAutoUnigaitCOM(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)13);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    callback(response.success(), response.message());
  } else {
    callback(false, "RPC failed: " + status_.error_message());
  }
}

void AdamCommand::SetErrorClearAsync(bool error_clear_flag,
                                     std::function<void(bool success, const std::string& message)> callback) {
  grpc::ClientContext context;
  adam_control::SetErrorClearRequest request;
  request.set_error_clear_flag(error_clear_flag);
  adam_control::SetErrorClearResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::SetErrorClearResponse>> rpc(
      stub_->AsyncSetErrorClear(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)14);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    callback(response.success(), response.message());
  } else {
    callback(false, "RPC failed: " + status_.error_message());
  }
}

void AdamCommand::GetStandListAsync(
    std::function<void(bool success, const std::vector<std::string>& modes, const std::string& message)> callback) {
  grpc::ClientContext context;
  adam_control::GetStandListRequest request;
  adam_control::GetStandListResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::GetStandListResponse>> rpc(
      stub_->AsyncGetStandList(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)17);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    std::vector<std::string> modes;
    for (const auto& mode : response.mode_list()) {
      modes.push_back(mode);
    }
    callback(response.success(), modes, response.message());
  } else {
    callback(false, {}, "RPC failed: " + status_.error_message());
  }
}

void AdamCommand::GetRobotStateAsync(
    std::function<
        void(bool success, const std::string& fsm_name, const std::string& current_motion,
             const std::vector<std::string>& current_action_list, const std::vector<std::string>& mode_enable_list,
             const std::vector<std::string>& motion_enable_list, const std::vector<std::string>& action_enable_list,
             const std::vector<std::string>& carrybox_enable_list, const std::string& balance_control_enable,
             double stand_pitch, double stand_roll, double stand_yaw, double stand_height, double x_vel, double y_vel,
             double yaw_vel, bool balance_control_state, bool motion_files_enable, const std::string& message)>
        callback) {
  grpc::ClientContext context;
  adam_control::GetRobotStateRequest request;
  adam_control::GetRobotStateResponse response;

  std::unique_ptr<grpc::ClientAsyncResponseReader<adam_control::GetRobotStateResponse>> rpc(
      stub_->AsyncGetRobotState(&context, request, &cq_));

  rpc->Finish(&response, &status_, (void*)18);

  cq_.Next(&tag_, &ok_);
  if (ok_ && status_.ok()) {
    std::vector<std::string> current_action_list(response.current_action_list().begin(),
                                                 response.current_action_list().end());
    std::vector<std::string> mode_enable_list(response.mode_enable_list().begin(), response.mode_enable_list().end());
    std::vector<std::string> motion_enable_list(response.motion_enable_list().begin(),
                                                response.motion_enable_list().end());
    std::vector<std::string> action_enable_list(response.action_enable_list().begin(),
                                                response.action_enable_list().end());
    std::vector<std::string> carrybox_enable_list(response.carrybox_enable_list().begin(),
                                                  response.carrybox_enable_list().end());
    callback(response.success(), response.fsm_name(), response.current_motion(), current_action_list, mode_enable_list,
             motion_enable_list, action_enable_list, carrybox_enable_list, response.balance_control_enable(),
             response.stand_pitch(), response.stand_roll(), response.stand_yaw(), response.stand_height(),
             response.x_vel(), response.y_vel(), response.yaw_vel(), response.balance_control_state(),
             response.motion_files_enable(), response.message());
  } else {
    callback(false, "", "", {}, {}, {}, {}, {}, "", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, false, false,
             "RPC failed: " + status_.error_message());
  }
}

bool executeCommand(AdamCommand& client, const std::vector<std::string>& tokens, std::string& message) {
  if (tokens.empty()) {
    message = "Error: empty command.";
    return false;
  }

  const std::string& command = tokens[0];
  bool success = false;

  try {
    if (command == "SetMode") {
      if (tokens.size() != 2) {
        throw std::runtime_error("Error: Invalid number of arguments for SetMode.");
      }
      success = client.SetMode(tokens[1], message);
    } else if (command == "SetStandMotion") {
      if (tokens.size() < 2) {
        throw std::runtime_error("Error: Invalid number of arguments for SetStandMotion.");
      }
      std::string motion;
      for (size_t i = 1; i < tokens.size(); ++i) {
        if (i > 1) {
          motion += " ";
        }
        motion += tokens[i];
      }
      success = client.SetStandMotion(motion, message);
    } else if (command == "SetStandCarryBox") {
      if (tokens.size() < 2) {
        throw std::runtime_error("Error: Invalid number of arguments for SetStandCarryBox.");
      }
      std::string carry_box;
      for (size_t i = 1; i < tokens.size(); ++i) {
        if (i > 1) {
          carry_box += " ";
        }
        carry_box += tokens[i];
      }
      success = client.SetStandCarryBox(carry_box, message);
    } else if (command == "SetStandAction") {
      if (tokens.size() != 5) {
        throw std::runtime_error("Error: Invalid number of arguments for SetStandAction.");
      }
      success = client.SetStandAction(std::stod(tokens[1]), std::stod(tokens[2]), std::stod(tokens[3]),
                                      std::stod(tokens[4]), message);
    } else if (command == "SetStandDynamic") {
      if (tokens.size() != 2) {
        throw std::runtime_error("Error: Invalid number of arguments for SetStandDynamic.");
      }
      const std::string& dynamic_stand_str = tokens[1];
      if (dynamic_stand_str != "true" && dynamic_stand_str != "false") {
        throw std::runtime_error("Error: Invalid parameter for SetStandDynamic. Must be 'true' or 'false'.");
      }
      success = client.SetStandDynamic(dynamic_stand_str == "true", message);
    } else if (command == "SetSpeed") {
      if (tokens.size() != 4) {
        throw std::runtime_error("Error: Invalid number of arguments for SetSpeed.");
      }
      success = client.SetSpeed(std::stod(tokens[1]), std::stod(tokens[2]), std::stod(tokens[3]), message);
    } else if (command == "AutoUnigaitCOM") {
      if (tokens.size() != 2) {
        throw std::runtime_error("Error: Invalid number of arguments for AutoUnigaitCOM.");
      }
      const std::string& unigait_mode_com_x_str = tokens[1];
      if (unigait_mode_com_x_str != "true" && unigait_mode_com_x_str != "false") {
        throw std::runtime_error("Error: Invalid parameter for AutoUnigaitCOM. Must be 'true' or 'false'.");
      }
      success = client.AutoUnigaitCOM(unigait_mode_com_x_str == "true", message);
    } else if (command == "SetControlMode") {
      if (tokens.size() != 2) {
        throw std::runtime_error("Error: Invalid number of arguments for SetControlMode. Usage: SetControlMode <0|1>");
      }
      const int domain_id = std::stoi(tokens[1]);
      if (domain_id != 0 && domain_id != 1) {
        throw std::runtime_error("Error: domain_id must be 0(Traditional) or 1(RL).");
      }
      int final_mode = -1;
      success = client.SetControlModeAndWait(domain_id, final_mode, message);
    } else {
      throw std::runtime_error("Unknown command: " + command);
    }
  } catch (const std::exception& e) {
    message = e.what();
    success = false;
  }

  return success;
}

}  // namespace adam_control
