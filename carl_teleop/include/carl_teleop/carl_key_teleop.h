/*
 * Copyright (c) 2011, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Willow Garage, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * \carl_key_teleop.h
 * \brief Allows for control of CARL with a keyboard.
 *
 * carl_joy_teleop creates a ROS node that allows the control of CARL with a keyboard.
 * This node listens to a /joy topic and sends messages to the /cmd_vel topic.
 *
 * \author Steven Kordell, WPI - spkordell@wpi.edu
 * \date May 23, 2014
 */

#ifndef CARL_JOY_TELEOP_H_
#define CARL_JOY_TELEOP_H_

#include <geometry_msgs/Twist.h>
#include <ros/ros.h>
#include <sensor_msgs/Joy.h>

#define MAX_TRANS_VEL .8
#define MAX_ANG_VEL 1.2

class carl_key_teleop
{
public:
  /*!
   * \brief Creates a carl_key_teleop.
   *
   * Creates a carl_key_teleop object that can be used control carl with a keyboard.
   * ROS nodes, services, and publishers are created and maintained within this object.
   */
  carl_key_teleop();

  /*!
   * \brief monitors the keyboard and publishes cmd_vels.
   *
   * Monitors the keyboard and publishes cmd_vels whenever a key corresponding to a motion
   * command is pressed.
   */
  void keyLoop();

  /*!
   * \brief Stops the robot when no key is pressed
   *
   * Publishes cmd_vels to stop the robot if no key is pressed after a short time period.
   */
  void watchdog();

private:
  ros::NodeHandle nh_, ph_;
  double linear_, angular_;
  ros::Time first_publish_;
  ros::Time last_publish_;
  double l_scale_, a_scale_;
  ros::Publisher vel_pub_;

  /*!
   * \brief Publishes a cmd_vels
   *
   * Helper function for publishing cmd_vels.
   */
  void publish(double, double);
  boost::mutex publish_mutex_;
};

/*!
 * Creates and runs the carl_key_teleop node.
 *
 * \param argc argument count that is passed to ros::init
 * \param argv arguments that are passed to ros::init
 * \return EXIT_SUCCESS if the node runs correctly
 */
int main(int argc, char **argv);

#endif