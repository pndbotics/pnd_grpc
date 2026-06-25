#ifndef ADAM_COMMAND_H
#define ADAM_COMMAND_H

#include <grpcpp/grpcpp.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "adam_control.grpc.pb.h"
#include "adam_control.pb.h"

namespace adam_control {

constexpr int kGrpcConnectTimeoutSec = 5;

std::string ControlModeName(int domain_id);

class AdamCommand {
 public:
  explicit AdamCommand(const std::string& server_address);

  bool SetMode(const std::string& mode, std::string& message);
  bool SetStandMotion(const std::string& motion, std::string& message);
  bool SetStandCarryBox(const std::string& carry_box, std::string& message);
  bool SetStandAction(double stand_pitch, double stand_roll, double stand_yaw, double stand_height,
                      std::string& message);
  bool SetStandDynamic(bool dynamic_stand, std::string& message);
  bool SetSpeed(double x_speed, double y_speed, double yaw_speed, std::string& message);
  bool AutoUnigaitCOM(bool unigait_mode_com_x, std::string& message);
  bool SetErrorClear(bool error_clear_flag, std::string& message);
  bool GetStandList(bool& stand_list_flag, std::vector<std::string>& mode_list,
                    std::vector<std::string>& motion_list, std::vector<std::string>& action_list,
                    std::vector<std::string>& carrybox_list, std::string& balance_control,
                    std::string& message);
  bool GetRobotState(bool& robot_state_flag, std::string& fsm_name, std::string& current_motion,
                     std::vector<std::string>& mode_enable_list, std::vector<std::string>& motion_enable_list,
                     std::vector<std::string>& action_enable_list,
                     std::vector<std::string>& carrybox_enable_list, std::string& balance_control_enable,
                     double& stand_pitch, double& stand_roll, double& stand_yaw, double& stand_height,
                     double& x_vel, double& y_vel, double& yaw_vel, bool& balance_control_state,
                     bool& motion_files_enable, int& current_control_mode, std::string& message);

  // domain_id: 0=Traditional MPC, 1=RL
  bool SetControlMode(int domain_id, int& confirmed_mode, std::string& message);
  bool GetControlState(int& domain_id, std::string& message);
  bool WaitForControlMode(int target_domain_id, int& final_mode, std::string& message,
                          double timeout_sec = 30.0, double poll_interval_sec = 0.5);
  bool SetControlModeAndWait(int domain_id, int& final_mode, std::string& message);

  void SetModeAsync(const std::string& mode, std::function<void(bool success, const std::string& message)> callback);
  void SetStandMotionAsync(const std::string& motion,
                           std::function<void(bool success, const std::string& message)> callback);
  void SetStandCarryBoxAsync(const std::string& carry_box,
                             std::function<void(bool success, const std::string& message)> callback);
  void SetStandActionAsync(double stand_pitch, double stand_roll, double stand_yaw, double stand_height,
                           std::function<void(bool success, const std::string& message)> callback);
  void SetStandDynamicAsync(bool dynamic_stand, std::function<void(bool success, const std::string& message)> callback);
  void SetSpeedAsync(double x_speed, double y_speed, double yaw_speed,
                     std::function<void(bool success, const std::string& message)> callback);
  void AutoUnigaitCOMAsync(bool unigait_mode_com_x,
                           std::function<void(bool success, const std::string& message)> callback);
  void SetErrorClearAsync(bool error_clear_flag,
                          std::function<void(bool success, const std::string& message)> callback);
  void GetStandListAsync(
      std::function<void(bool success, const std::vector<std::string>& modes, const std::string& message)> callback);
  void GetRobotStateAsync(
      std::function<
          void(bool success, const std::string& fsm_name, const std::string& current_motion,
               const std::vector<std::string>& current_action_list, const std::vector<std::string>& mode_enable_list,
               const std::vector<std::string>& motion_enable_list, const std::vector<std::string>& action_enable_list,
               const std::vector<std::string>& carrybox_enable_list, const std::string& balance_control_enable,
               double stand_pitch, double stand_roll, double stand_yaw, double stand_height, double x_vel, double y_vel,
               double yaw_vel, bool balance_control_state, bool motion_files_enable, const std::string& message)>
          callback);
  void SetControlModeAsync(int domain_id,
                           std::function<void(bool success, int confirmed_mode, const std::string& message)> callback);

 private:
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<adam_control::RobotControl::Stub> stub_;
  grpc::CompletionQueue cq_;
  void* tag_;
  bool ok_;
  grpc::Status status_;
};

bool executeCommand(AdamCommand& client, const std::vector<std::string>& tokens, std::string& message);

}  // namespace adam_control

#endif  // ADAM_COMMAND_H
