/*!********************************************************************************
 * \brief     Hover implementation
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

#include "../include/behavior_hover_with_mpc_control.h"
#include <pluginlib/class_list_macros.h>
#include <iostream>
#include <fstream>

namespace quadrotor_motion_with_mpc_control
{
BehaviorHoverWithMpcControl::BehaviorHoverWithMpcControl() : BehaviorExecutionController() { 
  setName("hover_with_mpc_control"); 
  setExecutionGoal(ExecutionGoals::KEEP_RUNNING); 
}

BehaviorHoverWithMpcControl::~BehaviorHoverWithMpcControl() {}

void BehaviorHoverWithMpcControl::onConfigure()
{
  node_handle = getNodeHandle();
  nspace = getNamespace(); 
}

bool BehaviorHoverWithMpcControl::checkSituation()
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

void BehaviorHoverWithMpcControl::onActivate()
{
  motion_reference_pose_pub = node_handle.advertise<geometry_msgs::PoseStamped>("/" + nspace + "/motion_reference/pose", 1,true);
  flight_state_pub = node_handle.advertise<aerostack_msgs::FlightState>("/" + nspace + "/self_localization/flight_state", 1, true);
  self_localization_speed_sub = node_handle.subscribe("/" + nspace + "/self_localization/speed", 1, &BehaviorHoverWithMpcControl::selfLocalizationSpeedCallBack, this);

  //Get current drone pose
  geometry_msgs::PoseStamped::ConstPtr sharedPose;
  geometry_msgs::PoseStamped drone_initial_pose;
  sharedPose = ros::topic::waitForMessage<geometry_msgs::PoseStamped>("/" + nspace + "/self_localization/pose",node_handle);
  if(sharedPose != NULL){
    drone_initial_pose = *sharedPose;
  }
  
  quadrotor_moving = true;
  received_speed = false;
  motion_reference_pose_pub.publish(drone_initial_pose);  
  aerostack_msgs::FlightState flight_state_msg;
  flight_state_msg.state = aerostack_msgs::FlightState::HOVERING;
  flight_state_pub.publish(flight_state_msg);
}

void BehaviorHoverWithMpcControl::onDeactivate()
{
  motion_reference_pose_pub.shutdown();
}

void BehaviorHoverWithMpcControl::onExecute()
{ 

}

void BehaviorHoverWithMpcControl::checkGoal(){}


void BehaviorHoverWithMpcControl::checkProgress() {
  if (!quadrotor_moving){
    distance = sqrt(pow(estimated_pose_msg.pose.position.x-reference_pose.pose.position.x,2)+
                    pow(estimated_pose_msg.pose.position.y-reference_pose.pose.position.y,2)+
                    pow(estimated_pose_msg.pose.position.z-reference_pose.pose.position.z,2));

    if (distance > 1) 
      BehaviorExecutionController::setTerminationCause(aerostack_msgs::BehaviorActivationFinished::WRONG_PROGRESS);
  }
}


void BehaviorHoverWithMpcControl::checkProcesses() 
{ 
 
}

bool BehaviorHoverWithMpcControl::checkQuadrotorStopped()
{
  if (abs(estimated_speed_msg.twist.linear.x) <= 0.1 && abs(estimated_speed_msg.twist.linear.y) <= 0.1 && abs(estimated_speed_msg.twist.linear.z) <= 0.1 &&
      abs(estimated_speed_msg.twist.angular.x) <= 0.1 && abs(estimated_speed_msg.twist.angular.y) <= 0.1 && abs(estimated_speed_msg.twist.angular.z) <= 0.1 ){
      return true;
  }else{
    return false;
  }
}

void BehaviorHoverWithMpcControl::selfLocalizationSpeedCallBack(const geometry_msgs::TwistStamped &msg){
  estimated_speed_msg = msg; received_speed = true;
}

}
PLUGINLIB_EXPORT_CLASS(quadrotor_motion_with_mpc_control::BehaviorHoverWithMpcControl, nodelet::Nodelet)