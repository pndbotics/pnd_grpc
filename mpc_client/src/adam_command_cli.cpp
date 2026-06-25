#include "../include/adam_command.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

void split(const std::string& s, char delimiter, std::vector<std::string>& tokens) {
  std::stringstream ss(s);
  std::string token;
  while (std::getline(ss, token, delimiter)) {
    tokens.push_back(token);
  }
}

void printHelp() {
  std::cout << "Available commands:" << std::endl;
  std::cout << "  SetMode" << std::endl;
  std::cout << "  SetStandMotion" << std::endl;
  std::cout << "  SetStandCarryBox" << std::endl;
  std::cout << "  SetStandAction" << std::endl;
  std::cout << "  SetStandDynamic" << std::endl;
  std::cout << "  SetSpeed" << std::endl;
  std::cout << "  AutoUnigaitCOM" << std::endl;
  std::cout << "  SetErrorClear" << std::endl;
  std::cout << "  GetStandList" << std::endl;
  std::cout << "  GetRobotState" << std::endl;
  std::cout << "  SetControlMode  <0=Traditional|1=RL>  -- switch control paradigm" << std::endl;
  std::cout << "  GetControlState                       -- query DDS rt/control_mode_state" << std::endl;
  std::cout << "  exit" << std::endl;
}

bool isPlaceholderIp(const std::string& ip) {
  return ip.empty() || ip == "xx.xx.xx.xx" || ip == "0.0.0.0";
}

}  // namespace

int main() {
  std::ifstream file("../ip_config.json");
  if (!file.is_open()) {
    std::cerr << "Error: Could not open ../ip_config.json" << std::endl;
    return 1;
  }

  json config;
  file >> config;
  file.close();

  if (config.find("server") == config.end() || config["server"].find("ip") == config["server"].end()) {
    std::cerr << "Error: Invalid config file format." << std::endl;
    return 1;
  }

  const std::string server_ip = config["server"]["ip"];
  if (isPlaceholderIp(server_ip)) {
    std::cerr << "请在 ip_config.json 中配置机器人实际 IP 地址（当前: " << server_ip << "）" << std::endl;
    return 1;
  }

  const int server_port = config["server"].value("port", 6666);
  const std::string server_address = server_ip + ":" + std::to_string(server_port);

  try {
    adam_control::AdamCommand client(server_address);

    std::cout << "Adam Command Client v1.1.0" << std::endl;
    std::cout << "Connected to gRPC server: " << server_address << std::endl;
    std::cout << "Type 'help' for usage information." << std::endl;

  enum class CommandState { WAIT_COMMAND, WAIT_PARAMETER, WAIT_SPEED_INPUT, WAIT_ACTION_INPUT };

  CommandState state = CommandState::WAIT_COMMAND;
  std::string pendingCommand;
  std::vector<std::string> enable_list;

  while (true) {
    std::string input;

    if (state == CommandState::WAIT_COMMAND) {
      std::cout << "> ";
    } else if (state == CommandState::WAIT_SPEED_INPUT) {
      std::cout << "Speed> Enter speed values (x y yaw) or type 'exit' to quit: ";
    } else if (state == CommandState::WAIT_ACTION_INPUT) {
      std::cout << "Action> Enter action values (stand_pitch stand_roll stand_yaw stand_height): ";
    } else {
      std::cout << "Enter parameter for " << pendingCommand << ": ";
    }

    if (!std::getline(std::cin, input)) {
      break;
    }

    if (input.empty() && state == CommandState::WAIT_COMMAND) {
      continue;
    }

    std::string fsm_name;
    std::string current_motion;
    std::string balance_control_enable;
    std::vector<std::string> mode_enable_list;
    std::vector<std::string> motion_enable_list;
    std::vector<std::string> action_enable_list;
    std::vector<std::string> carrybox_enable_list;
    double stand_pitch = 0.0;
    double stand_roll = 0.0;
    double stand_yaw = 0.0;
    double stand_height = 0.0;
    double x_vel = 0.0;
    double y_vel = 0.0;
    double yaw_vel = 0.0;
    bool balance_control_state = false;
    bool motion_files_enable = false;
    int current_control_mode = -1;
    std::string message;
    bool robot_state_flag = true;
    const bool success = client.GetRobotState(
        robot_state_flag, fsm_name, current_motion, mode_enable_list, motion_enable_list, action_enable_list,
        carrybox_enable_list, balance_control_enable, stand_pitch, stand_roll, stand_yaw, stand_height, x_vel, y_vel,
        yaw_vel, balance_control_state, motion_files_enable, current_control_mode, message);

    if (state == CommandState::WAIT_COMMAND) {
      if (input == "exit") {
        std::cout << "Exiting Adam Command Client." << std::endl;
        break;
      }
      if (input == "help") {
        printHelp();
        continue;
      }
      if (input == "SetMode") {
        if (!success) {
          std::cout << "Failed to get robot state: " << message << std::endl;
          continue;
        }
        enable_list = mode_enable_list;
        std::cout << "Available Modes: ";
        std::copy(enable_list.begin(), enable_list.end(), std::ostream_iterator<std::string>(std::cout, " "));
        std::cout << std::endl;
        pendingCommand = input;
        state = CommandState::WAIT_PARAMETER;
        continue;
      }
      if (input == "SetStandMotion" || input == "SetStandCarryBox") {
        if (!success) {
          std::cout << "Failed to get robot state: " << message << std::endl;
          continue;
        }
        if (fsm_name != "Stand") {
          std::cout << "Current mode is " << fsm_name
                    << ", this command can only be executed when mode is 'Stand'." << std::endl;
          continue;
        }
        if (input == "SetStandMotion") {
          enable_list = motion_enable_list;
          std::cout << "Available Motions: ";
        } else {
          enable_list = carrybox_enable_list;
          std::cout << "Available Carry Boxes: ";
        }
        std::copy(enable_list.begin(), enable_list.end(), std::ostream_iterator<std::string>(std::cout, ", "));
        std::cout << std::endl;
        pendingCommand = input;
        state = CommandState::WAIT_PARAMETER;
        continue;
      }
      if (input == "SetStandAction") {
        if (!success) {
          std::cout << "Failed to get robot state: " << message << std::endl;
          continue;
        }
        if (fsm_name != "Stand") {
          std::cout << "Current mode is " << fsm_name
                    << ", this command can only be executed when mode is 'Stand'." << std::endl;
          continue;
        }
        std::cout << "Current action values (stand_pitch stand_roll stand_yaw stand_height) (" << stand_pitch << ", "
                  << stand_roll << ", " << stand_yaw << ", " << stand_height << ")" << std::endl;
        std::cout << "Please enter values within the following ranges:" << std::endl;
        std::cout << "  - Pitch: [-0.1, 0.1]" << std::endl;
        std::cout << "  - Roll: [-0.06, 0.06]" << std::endl;
        std::cout << "  - Yaw: [-0.25, 0.25]" << std::endl;
        std::cout << "  - Base Height: [-0.2, 0.0]" << std::endl;
        pendingCommand = input;
        state = CommandState::WAIT_ACTION_INPUT;
        continue;
      }
      if (input == "SetStandDynamic") {
        if (!success) {
          std::cout << "Failed to get robot state: " << message << std::endl;
          continue;
        }
        if (fsm_name != "Stand") {
          std::cout << "Current mode is " << fsm_name
                    << ", this command can only be executed when mode is 'Stand'." << std::endl;
          continue;
        }
        std::cout << "Dynamic Stand State: " << (balance_control_state ? "true" : "false") << "." << std::endl;
        pendingCommand = input;
        state = CommandState::WAIT_PARAMETER;
        continue;
      }
      if (input == "SetSpeed" || input == "AutoUnigaitCOM") {
        if (!success) {
          std::cout << "Failed to get robot state: " << message << std::endl;
          continue;
        }
        if (fsm_name != "Walk" && fsm_name != "Run") {
          std::cout << "Current mode is " << fsm_name
                    << ", this command can only be executed when mode is 'Walk' or 'Run'." << std::endl;
          continue;
        }
        pendingCommand = input;
        if (input == "SetSpeed") {
          state = CommandState::WAIT_SPEED_INPUT;
        } else {
          state = CommandState::WAIT_PARAMETER;
        }
        continue;
      }
      if (input == "SetErrorClear") {
        if (fsm_name != "Stop") {
          std::cout << "Current mode is " << fsm_name
                    << ", this command can only be executed when mode is 'Stop'." << std::endl;
          continue;
        }
        if (client.SetErrorClear(true, message)) {
          std::cout << "Success: " << message << std::endl;
        } else {
          std::cout << "Failed: " << message << std::endl;
        }
        continue;
      }
      if (input == "SetControlMode") {
        std::cout << "Current control mode: " << current_control_mode << " ("
                  << adam_control::ControlModeName(current_control_mode) << ")" << std::endl;
        pendingCommand = input;
        state = CommandState::WAIT_PARAMETER;
        continue;
      }
      if (input == "GetControlState") {
        int domain_id = -1;
        std::string control_message;
        if (client.GetControlState(domain_id, control_message)) {
          std::cout << "Control State: domain_id=" << domain_id << " ("
                    << adam_control::ControlModeName(domain_id) << ")" << std::endl;
          if (!control_message.empty()) {
            std::cout << "  Message: " << control_message << std::endl;
          }
        } else {
          std::cout << "Failed: " << control_message << std::endl;
        }
        continue;
      }
      if (input == "GetStandList") {
        std::vector<std::string> mode_list;
        std::vector<std::string> motion_list;
        std::vector<std::string> action_list;
        std::vector<std::string> carrybox_list;
        std::string balance_control;
        bool stand_list_flag = true;
        if (client.GetStandList(stand_list_flag, mode_list, motion_list, action_list, carrybox_list, balance_control,
                                message)) {
          std::cout << "Modes: ";
          std::copy(mode_list.begin(), mode_list.end(), std::ostream_iterator<std::string>(std::cout, " "));
          std::cout << std::endl << "Motions: ";
          std::copy(motion_list.begin(), motion_list.end(), std::ostream_iterator<std::string>(std::cout, " "));
          std::cout << std::endl << "Actions: ";
          std::copy(action_list.begin(), action_list.end(), std::ostream_iterator<std::string>(std::cout, " "));
          std::cout << std::endl << "Carry Boxes: ";
          std::copy(carrybox_list.begin(), carrybox_list.end(), std::ostream_iterator<std::string>(std::cout, " "));
          std::cout << std::endl << "Balance Control: " << balance_control << std::endl;
        } else {
          std::cout << "Failed to get stand list: " << message << std::endl;
        }
        continue;
      }
      if (input == "GetRobotState") {
        if (success) {
          std::cout << "Current Mode: " << fsm_name << std::endl;
          std::cout << "Current Motion: " << current_motion << std::endl;
          std::cout << "Enable Mode List: ";
          std::copy(mode_enable_list.begin(), mode_enable_list.end(),
                    std::ostream_iterator<std::string>(std::cout, " "));
          std::cout << std::endl << "Enable Motion List: ";
          std::copy(motion_enable_list.begin(), motion_enable_list.end(),
                    std::ostream_iterator<std::string>(std::cout, " "));
          std::cout << std::endl << "Enable Action List: ";
          std::copy(action_enable_list.begin(), action_enable_list.end(),
                    std::ostream_iterator<std::string>(std::cout, " "));
          std::cout << std::endl << "Enable Carry Box List: ";
          std::copy(carrybox_enable_list.begin(), carrybox_enable_list.end(),
                    std::ostream_iterator<std::string>(std::cout, " "));
          std::cout << std::endl << "Balance Control Enable: " << balance_control_enable << std::endl;
          std::cout << "Stand Pitch: " << stand_pitch << std::endl;
          std::cout << "Stand Roll: " << stand_roll << std::endl;
          std::cout << "Stand Yaw: " << stand_yaw << std::endl;
          std::cout << "Stand Height: " << stand_height << std::endl;
          std::cout << "X Velocity: " << x_vel << std::endl;
          std::cout << "Y Velocity: " << y_vel << std::endl;
          std::cout << "Yaw Velocity: " << yaw_vel << std::endl;
          std::cout << "Balance Control State: " << balance_control_state << std::endl;
          std::cout << "Control Mode: " << current_control_mode << " ("
                    << adam_control::ControlModeName(current_control_mode) << ")" << std::endl;
        } else {
          std::cout << "Failed to get robot state: " << message << std::endl;
        }
        continue;
      }

      std::cout << "Unknown command: " << input << std::endl;
      continue;
    }

    if (state == CommandState::WAIT_PARAMETER) {
      if (input == "exit") {
        state = CommandState::WAIT_COMMAND;
        std::cout << "Exiting current command interface." << std::endl;
        continue;
      }

      if (pendingCommand == "SetControlMode") {
        try {
          const int domain_id = std::stoi(input);
          if (domain_id != 0 && domain_id != 1) {
            std::cout << "Error: Invalid control mode. Please enter 0 (Traditional) or 1 (RL)." << std::endl;
            state = CommandState::WAIT_COMMAND;
            continue;
          }
          int final_mode = -1;
          if (client.SetControlModeAndWait(domain_id, final_mode, message)) {
            std::cout << "Success: " << message << std::endl;
          } else {
            std::cout << "Failed: " << message << std::endl;
          }
        } catch (const std::exception&) {
          std::cout << "Error: Invalid control mode. Please enter 0 (Traditional) or 1 (RL)." << std::endl;
        }
        state = CommandState::WAIT_COMMAND;
        continue;
      }

      if ((pendingCommand == "SetMode" || pendingCommand == "SetStandMotion" ||
           pendingCommand == "SetStandCarryBox") &&
          std::find(enable_list.begin(), enable_list.end(), input) == enable_list.end()) {
        std::cout << "Error: Invalid parameter. Please enter a valid option from the enable list." << std::endl;
        state = CommandState::WAIT_COMMAND;
        continue;
      }

      if (pendingCommand == "SetStandDynamic" || pendingCommand == "AutoUnigaitCOM") {
        if (input != "true" && input != "false") {
          std::cout << "Error: Invalid parameter. Please enter 'true' or 'false'." << std::endl;
          continue;
        }
        if (pendingCommand == "SetStandDynamic") {
          const bool new_state = (input == "true");
          if (new_state == balance_control_state) {
            std::cout << "Already in the state: " << (balance_control_state ? "true" : "false")
                      << ". No change needed." << std::endl;
            continue;
          }
        }
      }

      std::vector<std::string> tokens;
      split(input, ' ', tokens);
      tokens.insert(tokens.begin(), pendingCommand);

      if (adam_control::executeCommand(client, tokens, message)) {
        std::cout << "Success: " << message << std::endl;
      } else {
        std::cout << "Failed: " << message << std::endl;
      }
      state = CommandState::WAIT_COMMAND;
      continue;
    }

    if (state == CommandState::WAIT_SPEED_INPUT) {
      if (input == "exit") {
        state = CommandState::WAIT_COMMAND;
        std::cout << "Exiting current command interface." << std::endl;
        continue;
      }

      std::vector<std::string> tokens;
      split(input, ' ', tokens);
      tokens.insert(tokens.begin(), "SetSpeed");

      if (adam_control::executeCommand(client, tokens, message)) {
        std::cout << "Success: " << message << std::endl;
      } else {
        std::cout << "Failed: " << message << std::endl;
      }
      continue;
    }

    if (state == CommandState::WAIT_ACTION_INPUT) {
      if (input == "exit") {
        state = CommandState::WAIT_COMMAND;
        std::cout << "Exiting current command interface." << std::endl;
        continue;
      }

      std::vector<std::string> tokens;
      split(input, ' ', tokens);
      if (tokens.size() != 4) {
        std::cout << "Error: Invalid number of arguments. Please enter four float values." << std::endl;
        continue;
      }

      try {
        const double pitch = std::stod(tokens[0]);
        const double roll = std::stod(tokens[1]);
        const double yaw = std::stod(tokens[2]);
        const double height = std::stod(tokens[3]);

        const double pitch_min = -0.1;
        const double pitch_max = 0.1;
        const double roll_min = -0.06;
        const double roll_max = 0.06;
        const double yaw_min = -0.25;
        const double yaw_max = 0.25;
        const double height_min = -0.2;
        const double height_max = 0.0;

        bool out_of_range = false;
        if (pitch < pitch_min || pitch > pitch_max) {
          std::cout << "Error: Pitch (" << pitch << ") is out of range." << std::endl;
          out_of_range = true;
        }
        if (roll < roll_min || roll > roll_max) {
          std::cout << "Error: Roll (" << roll << ") is out of range." << std::endl;
          out_of_range = true;
        }
        if (yaw < yaw_min || yaw > yaw_max) {
          std::cout << "Error: Yaw (" << yaw << ") is out of range." << std::endl;
          out_of_range = true;
        }
        if (height < height_min || height > height_max) {
          std::cout << "Error: Height (" << height << ") is out of range." << std::endl;
          out_of_range = true;
        }
        if (out_of_range) {
          std::cout << "Please enter values within the documented ranges." << std::endl;
          continue;
        }

        tokens.insert(tokens.begin(), "SetStandAction");
        if (adam_control::executeCommand(client, tokens, message)) {
          std::cout << "Success: " << message << std::endl;
        } else {
          std::cout << "Failed: " << message << std::endl;
        }
      } catch (const std::exception&) {
        std::cout << "Error: Invalid input. Please enter four float values." << std::endl;
      }
    }
  }

  return 0;
  } catch (const std::exception& e) {
    std::cerr << "连接失败: " << e.what() << std::endl;
    return 1;
  }
}
