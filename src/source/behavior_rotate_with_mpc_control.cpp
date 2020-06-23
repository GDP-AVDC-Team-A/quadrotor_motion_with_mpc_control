/*!********************************************************************************
 * \brief     rotate implementation
 * \authors   Alberto Rodelgo
 * \copyright Copyright (c) 2020 Universidad Politecnica de Madrid
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************/

#include "../include/behavior_rotate_with_mpc_control.h"
#include <pluginlib/class_list_macros.h>

namespace quadrotor_motion_with_mpc_control
{
BehaviorRotateWithMpcControl::BehaviorRotateWithMpcControl() : BehaviorExecutionController() { 
  setName("rotate_with_mpc_control"); 
  setExecutionGoal(ExecutionGoals::ACHIEVE_GOAL);
}

BehaviorRotateWithMpcControl::~BehaviorRotateWithMpcControl() {}

void BehaviorRotateWithMpcControl::onConfigure()
{
  node_handle = getNodeHandle();
  nspace = getNamespace(); 

}

bool BehaviorRotateWithMpcControl::checkSituation()
{
  geometry_msgs::PoseStamped::ConstPtr sharedPose;
  geometry_msgs::PoseStamped drone_initial_pose;
  sharedPose = ros::topic::waitForMessage<geometry_msgs::PoseStamped>("/" + nspace + "/self_localization/pose",node_handle);
  if(sharedPose != NULL){
    drone_initial_pose = *sharedPose;
  }

  //Quadrotor is FLYING
  if (drone_initial_pose.pose.position.z > 0.2){
    return true;
  }
  else{
    setErrorMessage("Error: Drone is not flying");
    std::cout<<"Error: Drone is not flying"<< std::endl;
    return false;
  }
}

void BehaviorRotateWithMpcControl::checkGoal(){ 
  if (!quadrotor_moving){
      current_angle = 0;
      angle2 = 0;
      if(!(estimated_pose_msg.pose.orientation.w == 0 && estimated_pose_msg.pose.orientation.x == 0 && estimated_pose_msg.pose.orientation.y == 0 && estimated_pose_msg.pose.orientation.z == 0)){
          current_angle = atan2(2.0 * (estimated_pose_msg.pose.orientation.z * estimated_pose_msg.pose.orientation.w + estimated_pose_msg.pose.orientation.x * estimated_pose_msg.pose.orientation.y) , 
                              - 1.0 + 2.0 * (estimated_pose_msg.pose.orientation.w * estimated_pose_msg.pose.orientation.w + estimated_pose_msg.pose.orientation.x * estimated_pose_msg.pose.orientation.x));    
          angle2 = atan2(2.0 * (reference_pose.pose.orientation.z * reference_pose.pose.orientation.w + reference_pose.pose.orientation.x * reference_pose.pose.orientation.y) , 
                              - 1.0 + 2.0 * (reference_pose.pose.orientation.w * reference_pose.pose.orientation.w + reference_pose.pose.orientation.x * reference_pose.pose.orientation.x));    
      } 
      if (abs(abs(angle2) - abs(current_angle)) < 0.01 && abs(estimated_speed_msg.twist.angular.x) <= 0.1) BehaviorExecutionController::setTerminationCause(aerostack_msgs::BehaviorActivationFinished::GOAL_ACHIEVED);
  }
}

void BehaviorRotateWithMpcControl::checkProgress() {
  if (!quadrotor_moving){
    distance = sqrt(pow(estimated_pose_msg.pose.position.x-reference_pose.pose.position.x,2)+
                    pow(estimated_pose_msg.pose.position.y-reference_pose.pose.position.y,2)+
                    pow(estimated_pose_msg.pose.position.z-reference_pose.pose.position.z,2));

    if (distance > 1) BehaviorExecutionController::setTerminationCause(aerostack_msgs::BehaviorActivationFinished::WRONG_PROGRESS);
  }
}

void BehaviorRotateWithMpcControl::checkProcesses() 
{ 
 
}


void BehaviorRotateWithMpcControl::onActivate()
{
  //Subscribers
  self_localization_pose_sub = node_handle.subscribe("/" + nspace + "/self_localization/pose", 1, &BehaviorRotateWithMpcControl::selfLocalizationPoseCallBack, this);
  self_localization_speed_sub = node_handle.subscribe("/" + nspace + "/self_localization/speed", 1, &BehaviorRotateWithMpcControl::selfLocalizationSpeedCallBack, this);
  //Publishers
  motion_reference_pose_pub = node_handle.advertise<geometry_msgs::PoseStamped>("/" + nspace + "/motion_reference/pose", 1,true);
  flight_state_pub = node_handle.advertise<aerostack_msgs::FlightState>("/" + nspace + "/self_localization/flight_state", 1, true);

  //Get current drone pose
  geometry_msgs::PoseStamped::ConstPtr sharedPose;
  geometry_msgs::PoseStamped drone_initial_pose;
  sharedPose = ros::topic::waitForMessage<geometry_msgs::PoseStamped>("/" + nspace + "/self_localization/pose",node_handle);
  if(sharedPose != NULL){
    drone_initial_pose = *sharedPose;
  }

  quadrotor_moving = true;
  received_speed = false;

  // Extract target yaw
  std::string arguments=getParameters();
  YAML::Node config_file = YAML::Load(arguments);
  if(config_file["angle"].IsDefined())
  {
    angle=config_file["angle"].as<double>() * M_PI/180;
    q_rot.setRPY(0, 0, angle);
    reference_pose.pose.orientation.w = q_rot.getW();
    reference_pose.pose.orientation.x = q_rot.getX();
    reference_pose.pose.orientation.y = q_rot.getY();
    reference_pose.pose.orientation.z = q_rot.getZ();
  }
  else
  {
    if(config_file["relative_angle"].IsDefined())
    {
      angle=config_file["relative_angle"].as<double>() * M_PI/180;
      if(!(drone_initial_pose.pose.orientation.w == 0 && drone_initial_pose.pose.orientation.x == 0 && drone_initial_pose.pose.orientation.y == 0 && drone_initial_pose.pose.orientation.z == 0)){
          angle = angle + atan2(2.0 * (drone_initial_pose.pose.orientation.z * drone_initial_pose.pose.orientation.w + drone_initial_pose.pose.orientation.x * drone_initial_pose.pose.orientation.y) , 
                              - 1.0 + 2.0 * (drone_initial_pose.pose.orientation.w * drone_initial_pose.pose.orientation.w + drone_initial_pose.pose.orientation.x * drone_initial_pose.pose.orientation.x));    
      }
      if(angle > 2* M_PI || angle < -2*M_PI)
      {
        angle=angle*180/M_PI;
        angle= fmod(angle,360);
        angle=angle*M_PI/180;
      }
      q_rot.setRPY(0, 0, angle);
      reference_pose.pose.orientation.w = q_rot.getW();
      reference_pose.pose.orientation.x = q_rot.getX();
      reference_pose.pose.orientation.y = q_rot.getY();
      reference_pose.pose.orientation.z = q_rot.getZ();
    }
  }
  reference_pose.pose.position = drone_initial_pose.pose.position;
  motion_reference_pose_pub.publish(reference_pose);
  //Change flight state
  aerostack_msgs::FlightState flight_state_msg;
  flight_state_msg.state = aerostack_msgs::FlightState::FLYING;
  flight_state_pub.publish(flight_state_msg);
}

void BehaviorRotateWithMpcControl::onDeactivate()
{
  self_localization_pose_sub.shutdown();
  motion_reference_pose_pub.shutdown();
}

bool BehaviorRotateWithMpcControl::checkQuadrotorStopped()
{
  if (received_speed){
      if (abs(estimated_speed_msg.twist.linear.x) <= 0.15 && abs(estimated_speed_msg.twist.linear.y) <= 0.15 && abs(estimated_speed_msg.twist.linear.z) <= 0.15){
          return true;
      }else{
        return false;
      }
    }else{
      return true;
    }    
}

void BehaviorRotateWithMpcControl::selfLocalizationSpeedCallBack(const geometry_msgs::TwistStamped &msg){
  estimated_speed_msg = msg; received_speed = true;
}
void BehaviorRotateWithMpcControl::selfLocalizationPoseCallBack(const geometry_msgs::PoseStamped &msg){
  estimated_pose_msg = msg;
}

void BehaviorRotateWithMpcControl::onExecute()
{

}

}
PLUGINLIB_EXPORT_CLASS(quadrotor_motion_with_mpc_control::BehaviorRotateWithMpcControl, nodelet::Nodelet)
