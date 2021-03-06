/*!
 * \carl_joy_teleop.cpp
 * \brief Allows for control of CARL with a joystick.
 *
 * carl_joy_teleop creates a ROS node that allows the control of CARL with a joystick. 
 * This node listens to a /joy topic and sends messages to the /cmd_vel topic for 
 * the base and angular_cmd and cartesian_cmd for the arm.
 *
 * \author David Kent, WPI - davidkent@wpi.edu
 * \author Russell Toris, WPI - rctoris@wpi.edu
 * \author Steven Kordell, WPI - spkordell@wpi.edu
 * \date July 24, 2014
 */

#include <carl_teleop/carl_joy_teleop.h>

using namespace std;

carl_joy_teleop::carl_joy_teleop()
{
  // a private handle for this ROS node (allows retrieval of relative parameters)
  ros::NodeHandle private_nh("~");

  // create the ROS topics
  cmd_vel = node.advertise<geometry_msgs::Twist>("cmd_vel", 10);
  angular_cmd = node.advertise<wpi_jaco_msgs::AngularCommand>("jaco_arm/angular_cmd", 10);
  cartesian_cmd = node.advertise<wpi_jaco_msgs::CartesianCommand>("jaco_arm/cartesian_cmd", 10);
  asus_servo_tilt_cmd = node.advertise<std_msgs::Float64>("asus_controller/tilt", 10);
  creative_servo_pan_cmd = node.advertise<std_msgs::Float64>("creative_controller/pan", 10);
  joy_sub = node.subscribe<sensor_msgs::Joy>("joy", 10, &carl_joy_teleop::joy_cback, this);

  // read in throttle values
  private_nh.param<double>("linear_throttle_factor_base", linear_throttle_factor_base, 1.0);
  private_nh.param<double>("angular_throttle_factor_base", angular_throttle_factor_base, 1.0);
  private_nh.param<double>("linear_throttle_factor_arm", linear_throttle_factor_arm, 1.0);
  private_nh.param<double>("angular_throttle_factor_arm", angular_throttle_factor_arm, 1.0);
  private_nh.param<double>("finger_throttle_factor", finger_throttle_factor, 1.0);
  string str;
  private_nh.param<string>("controller_type", str, "digital");
  if (str.compare("digital") == 0)
    controllerType = DIGITAL;
  else
    controllerType = ANALOG;

  // initialize everything
  deadman = false;
  stopMessageSentArm = true;
  stopMessageSentFinger = true;
  EStopEnabled = false;
  helpDisplayed = false;
  mode = BASE_CONTROL;
  angularCmd.position = false;
  angularCmd.armCommand = false;
  angularCmd.fingerCommand = true;
  angularCmd.repeat = true;
  angularCmd.fingers.resize(3);
  cartesianCmd.position = false;
  cartesianCmd.armCommand = true;
  cartesianCmd.fingerCommand = false;
  cartesianCmd.repeat = true;

  ROS_INFO("CARL Joystick Teleop Started");

  puts(" ---------------------------------------------------");
  puts("| CARL Joystick Teleop Help                         |");
  puts("|---------------------------------------------------|*");
  if (mode == BASE_CONTROL)
    puts("| Current Mode: Base Control                        |*");
  else if (mode == ARM_CONTROL)
    puts("| Current Mode: Arm Control                         |*");
  else if (mode == FINGER_CONTROL)
    puts("| Current Mode: Finger Control                      |*");
  puts("|---------------------------------------------------|*");
  puts("| For help and controls, press:                     |*");
  puts("|                          _                        |*");
  puts("|                        _| |_                      |*");
  puts("|  show finger controls |_   _| show base controls  |*");
  puts("|                         |_|                       |*");
  puts("|                  show arm controls                |*");
  puts("|                                                   |*");
  puts(" ---------------------------------------------------**");
  puts("  ****************************************************");

  if (controllerType == ANALOG)
  {
    initLeftTrigger = false;
    initRightTrigger = false;
    calibrated = false;

    ROS_INFO(
        "You specified a controller with analog triggers. This requires calibration before any teleoperation can begin.  Please press and release both triggers before continuing.");
  }
  else
    calibrated = true;
}

void carl_joy_teleop::joy_cback(const sensor_msgs::Joy::ConstPtr& joy)
{
  // make sure triggers are calibrated before continuing if an analog controller was specified
  if (!calibrated)
  {
    if (!initLeftTrigger && joy->axes.at(2) == 1.0)
      initLeftTrigger = true;

    if (!initRightTrigger && joy->axes.at(5) == 1.0)
      initRightTrigger = true;

    if (initLeftTrigger && initRightTrigger)
    {
      calibrated = true;
      ROS_INFO("Controller calibration complete!");
    }

    return;
  }

  //software emergency stop for arm
  if (controllerType == DIGITAL)
  {
    if (joy->buttons.at(8) == 1)
      EStopEnabled = true;
    else if (joy->buttons.at(9) == 1)
      EStopEnabled = false;
  }
  else
  {
    if (joy->buttons.at(6) == 1)
      EStopEnabled = true;
    else if (joy->buttons.at(7) == 1)
      EStopEnabled = false;
  }

  //help menu
  if ((controllerType == DIGITAL && joy->axes.at(5) == -1.0) || (controllerType == ANALOG && joy->axes.at(7) == -1.0))
  {
    if (!helpDisplayed)
    {
      helpDisplayed = true;
      displayHelp(1);
    }
  }
  else if ((controllerType == DIGITAL && joy->axes.at(4) == 1.0)
      || (controllerType == ANALOG && joy->axes.at(6) == 1.0))
  {
    if (!helpDisplayed)
    {
      helpDisplayed = true;
      displayHelp(2);
    }
  }
  else if ((controllerType == DIGITAL && joy->axes.at(4) == -1.0)
      || (controllerType == ANALOG && joy->axes.at(6) == -1.0))
  {
    if (!helpDisplayed)
    {
      helpDisplayed = true;
      displayHelp(3);
    }
  }
  else if ((controllerType == DIGITAL && joy->axes.at(5) == 1.0)
      || (controllerType == ANALOG && joy->axes.at(7) == 1.0))
  {
    if (!helpDisplayed)
    {
      helpDisplayed = true;
      displayHelp(4);
    }
  }
  else
    helpDisplayed = false;

  //setup button indices for mode switching
  int button1Index, button2Index, button3Index, button4Index;
  bool modeChange;
  if (controllerType == DIGITAL)
  {
    button1Index = 0;
    button2Index = 1;
    button3Index = 2;
    button4Index = 3;
  }
  else
  {
    button1Index = 2;
    button2Index = 0;
    button3Index = 1;
    button4Index = 3;
  }

  bool was_pressed;

  switch (mode)
  {
    case BASE_CONTROL:
      // save the deadman switch state
      was_pressed = deadman;

      if (joy->buttons.at(4) == 1)
      {
        // left joystick controls the linear and angular movement
        twist.linear.x = joy->axes.at(1) * MAX_TRANS_VEL_BASE * linear_throttle_factor_base;
        if (controllerType == DIGITAL)
          twist.angular.z = joy->axes.at(2) * MAX_ANG_VEL_BASE * angular_throttle_factor_base;
        else
          twist.angular.z = joy->axes.at(3) * MAX_ANG_VEL_BASE * angular_throttle_factor_base;

        //boost throttle
        if (joy->buttons.at(5) != 1)
        {
          twist.linear.x *= NON_BOOST_THROTTLE;
          twist.angular.z *= NON_BOOST_THROTTLE;
        }
        deadman = true;
      }
      else
        deadman = false;

      //mode switching
      modeChange = false;
      if (joy->buttons.at(button1Index) == 1)
      {
        modeChange = true;
        mode = FINGER_CONTROL;
        ROS_INFO("Activated finger control mode");
      }
      else if (joy->buttons.at(button2Index) == 1)
      {
        modeChange = true;
        mode = ARM_CONTROL;
        ROS_INFO("Activated arm control mode");
      }
      else if (joy->buttons.at(button4Index) == 1)
      {
        modeChange = true;
        mode = SENSOR_CONTROL;
        ROS_INFO("Activated sensor control mode");
      }

      if (modeChange)
      {
        //cancel base motion
        twist.linear.x = 0.0;
        twist.angular.z = 0.0;
      }

      // send the twist command
      if (deadman || was_pressed)
        cmd_vel.publish(twist);

      break;
    case ARM_CONTROL:
      // left joystick controls the linear x and y movement
      cartesianCmd.arm.linear.x = joy->axes.at(1) * MAX_TRANS_VEL_ARM * linear_throttle_factor_arm;
      cartesianCmd.arm.linear.y = joy->axes.at(0) * MAX_TRANS_VEL_ARM * linear_throttle_factor_arm;

      //triggers control the linear z movement
      if (controllerType == DIGITAL)
      {
        if (joy->buttons.at(7) == 1)
          cartesianCmd.arm.linear.z = MAX_TRANS_VEL_ARM * linear_throttle_factor_arm;
        else if (joy->buttons.at(6) == 1)
          cartesianCmd.arm.linear.z = -MAX_TRANS_VEL_ARM * linear_throttle_factor_arm;
        else
          cartesianCmd.arm.linear.z = 0.0;
      }
      else
      {
        if (joy->axes.at(5) < 1.0)
          cartesianCmd.arm.linear.z = (0.5 - joy->axes.at(5) / 2.0) * MAX_ANG_VEL_ARM * angular_throttle_factor_arm;
        else
          cartesianCmd.arm.linear.z = -(0.5 - joy->axes.at(2) / 2.0) * MAX_ANG_VEL_ARM * angular_throttle_factor_arm;
      }

      //bumpers control roll
      if (joy->buttons.at(5) == 1)
        cartesianCmd.arm.angular.z = MAX_ANG_VEL_ARM * angular_throttle_factor_arm;
      else if (joy->buttons.at(4) == 1)
        cartesianCmd.arm.angular.z = -MAX_ANG_VEL_ARM * angular_throttle_factor_arm;
      else
        cartesianCmd.arm.angular.z = 0.0;

      //right joystick controls pitch and yaw
      if (controllerType == DIGITAL)
      {
        cartesianCmd.arm.angular.x = -joy->axes.at(3) * MAX_ANG_VEL_ARM * angular_throttle_factor_arm;
        cartesianCmd.arm.angular.y = joy->axes.at(2) * MAX_ANG_VEL_ARM * angular_throttle_factor_arm;
      }
      else
      {
        cartesianCmd.arm.angular.x = -joy->axes.at(4) * MAX_ANG_VEL_ARM * angular_throttle_factor_arm;
        cartesianCmd.arm.angular.y = joy->axes.at(3) * MAX_ANG_VEL_ARM * angular_throttle_factor_arm;
      }

      //mode switching
      if (joy->buttons.at(button1Index) == 1 || joy->buttons.at(button3Index) == 1 || joy->buttons.at(button4Index) == 1)
      {
        //cancel arm trajectory
        cartesianCmd.arm.linear.x = 0.0;
        cartesianCmd.arm.linear.y = 0.0;
        cartesianCmd.arm.linear.z = 0.0;
        cartesianCmd.arm.angular.x = 0.0;
        cartesianCmd.arm.angular.y = 0.0;
        cartesianCmd.arm.angular.z = 0.0;
        cartesian_cmd.publish(cartesianCmd);
        
        if (joy->buttons.at(button1Index) == 1)
        {
          mode = FINGER_CONTROL;
          ROS_INFO("Activated finger control mode");
        }
        else if (joy->buttons.at(button3Index) == 1)
        {
          mode = BASE_CONTROL;
          ROS_INFO("Activated base control mode");
        }
        else if (joy->buttons.at(button4Index) == 1)
        {
          mode = SENSOR_CONTROL;
          ROS_INFO("Activate sensor control mode");
        }
      }
      break;
    case FINGER_CONTROL:
      if (joy->axes.at(1) == 0.0)
      {
        //individual finger control
        //thumb controlled by right thumbstick
        if (controllerType == DIGITAL)
          angularCmd.fingers[0] = -joy->axes.at(3) * MAX_FINGER_VEL * finger_throttle_factor;
        else
          angularCmd.fingers[0] = -joy->axes.at(4) * MAX_FINGER_VEL * finger_throttle_factor;

        //top finger controlled by left triggers
        if (controllerType == DIGITAL)
        {
          if (joy->buttons.at(4) == 1)
            angularCmd.fingers[1] = -MAX_FINGER_VEL * finger_throttle_factor;
          else if (joy->buttons.at(6) == 1)
            angularCmd.fingers[1] = MAX_FINGER_VEL * finger_throttle_factor;
          else
            angularCmd.fingers[1] = 0.0;
        }
        else
        {
          if (joy->buttons.at(4) == 1)
            angularCmd.fingers[1] = -MAX_FINGER_VEL * finger_throttle_factor;
          else
            angularCmd.fingers[1] = (0.5 - joy->axes.at(2) / 2.0) * MAX_FINGER_VEL * finger_throttle_factor;
        }

        //bottom finger controlled by right bumpers
        if (controllerType == DIGITAL)
        {
          if (joy->buttons.at(5) == 1)
            angularCmd.fingers[2] = -MAX_FINGER_VEL * finger_throttle_factor;
          else if (joy->buttons.at(7) == 1)
            angularCmd.fingers[2] = MAX_FINGER_VEL * finger_throttle_factor;
          else
            angularCmd.fingers[2] = 0.0;
        }
        else
        {
          if (joy->buttons.at(5) == 1)
            angularCmd.fingers[2] = -MAX_FINGER_VEL * finger_throttle_factor;
          else
            angularCmd.fingers[2] = (0.5 - joy->axes.at(5) / 2.0) * MAX_FINGER_VEL * finger_throttle_factor;
        }
      }
      else
      {
        //control full gripper (outprioritizes individual finger control)
        angularCmd.fingers[0] = -joy->axes.at(1) * MAX_FINGER_VEL * finger_throttle_factor;
        angularCmd.fingers[1] = angularCmd.fingers[0];
        angularCmd.fingers[2] = angularCmd.fingers[0];
      }

      //mode switching
      if (joy->buttons.at(button2Index) == 1 || joy->buttons.at(button3Index) == 1 || joy->buttons.at(button4Index) == 1)
      {
        //cancel finger trajectory
        angularCmd.fingers[0] = 0.0;
        angularCmd.fingers[1] = 0.0;
        angularCmd.fingers[2] = 0.0;
        angular_cmd.publish(angularCmd);
        
        if (joy->buttons.at(button2Index) == 1)
        {
          mode = ARM_CONTROL;
          ROS_INFO("Activated arm control mode");
        }
        else if (joy->buttons.at(button3Index) == 1)
        {
          mode=BASE_CONTROL;
          ROS_INFO("Activated base control mode");
        }
        else if (joy->buttons.at(button4Index) == 1)
        {
          mode = SENSOR_CONTROL;
          ROS_INFO("Activate sensor control mode");
        }
      }
    break;
    case SENSOR_CONTROL:
      std_msgs::Float64 cameraTiltCommand;
      std_msgs::Float64 cameraPanCommand;

      //asus tilt      
      if (controllerType == DIGITAL)
        cameraTiltCommand.data = joy->axes.at(3) * 10; //scaled up for smoother movement
      else
        cameraTiltCommand.data = joy->axes.at(4) * 10; //scaled up for smoother movement
      if (cameraTiltCommand.data != 0)
      {
        asus_servo_tilt_cmd.publish(cameraTiltCommand);
      }
      
      //creative pan
      cameraPanCommand.data = joy->axes.at(0) * 10; //scaled up for smoother movement
      if (cameraPanCommand.data != 0)
      {
        creative_servo_pan_cmd.publish(cameraPanCommand);
      }
      
      //mode switch
      if (joy->buttons.at(button2Index) == 1)
      {
        mode = ARM_CONTROL;
        ROS_INFO("Activated arm control mode");
      }
      else if (joy->buttons.at(button2Index) == 1)
      {
        mode = FINGER_CONTROL;
        ROS_INFO("Activate finger control mode");
      }
      else if (joy->buttons.at(button3Index) == 1)
      {
        mode=BASE_CONTROL;
        ROS_INFO("Activated base control mode");
      }
      
    break;
  }
}

void carl_joy_teleop::publish_velocity()
{
  //publish arm stop commands if EStop is enabled (this will stop any teleoperation
  //using this node, but for any other nodes controlling the arm this will
  //instead significantly slow down any motion)
  if (EStopEnabled)
  {
    cartesianCmd.arm.linear.x = 0.0;
    cartesianCmd.arm.linear.y = 0.0;
    cartesianCmd.arm.linear.z = 0.0;
    cartesianCmd.arm.angular.x = 0.0;
    cartesianCmd.arm.angular.y = 0.0;
    cartesianCmd.arm.angular.z = 0.0;
    angularCmd.fingers[0] = 0.0;
    angularCmd.fingers[1] = 0.0;
    angularCmd.fingers[2] = 0.0;

    cartesian_cmd.publish(cartesianCmd);
    angular_cmd.publish(angularCmd);

    return;
  }

  switch (mode)
  {
    case ARM_CONTROL:
      //only publish stop message once; this allows other nodes to publish velocities
      //while the controller is not being used
      if (cartesianCmd.arm.linear.x == 0.0 && cartesianCmd.arm.linear.y == 0.0 && cartesianCmd.arm.linear.z == 0.0
          && cartesianCmd.arm.angular.x == 0.0 && cartesianCmd.arm.angular.y == 0.0
          && cartesianCmd.arm.angular.z == 0.0)
      {
        if (!stopMessageSentArm)
        {
          cartesian_cmd.publish(cartesianCmd);
          stopMessageSentArm = true;
        }
      }
      else
      {
        // send the arm command
        cartesian_cmd.publish(cartesianCmd);
        stopMessageSentArm = false;
      }
      break;
    case FINGER_CONTROL:
      //only publish stop message once; this allows other nodes to publish velocities
      //while the controller is not being used
      if (angularCmd.fingers[0] == 0.0 && angularCmd.fingers[1] == 0.0 && angularCmd.fingers[2] == 0.0)
      {
        if (!stopMessageSentFinger)
        {
          angular_cmd.publish(angularCmd);
          stopMessageSentFinger = true;
        }
      }
      else
      {
        //send the finger velocity command
        angular_cmd.publish(angularCmd);
        stopMessageSentFinger = false;
      }
      break;
  }
}

void carl_joy_teleop::displayHelp(int menuNumber)
{
  puts(" ----------------------------------------");
  puts("| CARL Joystick Teleop Help              |");
  puts("|----------------------------------------|*");
  if (mode == ARM_CONTROL)
    puts("| Current Mode: Arm Control              |*");
  else if (mode == FINGER_CONTROL)
    puts("| Current Mode: Finger Control           |*");
  else if (mode == BASE_CONTROL)
    puts(" Current Mode: Base Control              |*");
  puts("|----------------------------------------|*");
  switch (menuNumber)
  {
    case 1:
      puts("|              Arm Controls              |*");
      puts("|   roll/down                 roll/up    |*");
      puts("|    ________                ________    |*");
      puts("|   /    _   \\______________/        \\   |*");
      puts("|  |   _| |_    < >    < >     (4)    |  |*");
      puts("|  |  |_   _|  Estop  start (1)   (3) |  |*");
      puts("|  |    |_|    ___      ___    (2)    |  |*");
      puts("|  |          /   \\    /   \\          |  |*");
      puts("|  |          \\___/    \\___/          |  |*");
      puts("|  |       x/y trans  pitch/yaw       |  |*");
      puts("|  |        _______/--\\_______        |  |*");
    break;
    case 2:
      puts("|            Finger Controls             |*");
      puts("| finger1 open/close  finger2 open/close |*");
      puts("|    ________                ________    |*");
      puts("|   /    _   \\______________/        \\   |*");
      puts("|  |   _| |_    < >    < >     (4)    |  |*");
      puts("|  |  |_   _|  Estop  start (1)   (3) |  |*");
      puts("|  |    |_|    ___      ___    (2)    |  |*");
      puts("|  |          /   \\    /   \\          |  |*");
      puts("|  |          \\___/    \\___/          |  |*");
      puts("|  | hand open/close  thumb open/close|  |*");
      puts("|  |        _______/--\\_______        |  |*");
    break;
    case 3:
      puts("|             Base Controls              |*");
      puts("| deadman/no function  boost/no function |*");
      puts("|    ________                ________    |*");
      puts("|   /    _   \\______________/        \\   |*");
      puts("|  |   _| |_    < >    < >     (4)    |  |*");
      puts("|  |  |_   _|  Estop  start (1)   (3) |  |*");
      puts("|  |    |_|    ___      ___    (2)    |  |*");
      puts("|  |          /   \\    /   \\          |  |*");
      puts("|  |          \\___/    \\___/          |  |*");
      puts("|  |        fwd/bkwd  left/right      |  |*");
      puts("|  |        _______/--\\_______        |  |*");
    break;
    case 4:
      puts("|            Sensor Controls             |*");
      puts("|                                        |*");
      puts("|    ________                ________    |*");
      puts("|   /    _   \\______________/        \\   |*");
      puts("|  |   _| |_    < >    < >     (4)    |  |*");
      puts("|  |  |_   _|  Estop  start (1)   (3) |  |*");
      puts("|  |    |_|    ___      ___    (2)    |  |*");
      puts("|  |          /   \\    /   \\          |  |*");
      puts("|  |          \\___/    \\___/          |  |*");
      puts("|  |                  asus tilt       |  |*");
      puts("|  |        _______/--\\_______        |  |*");
    break;
  }
  puts("|  |       |                  |       |  |*");
  puts("|   \\     /                    \\     /   |*");
  puts("|    \\___/                      \\___/    |*");
  puts("|                                        |*");
  puts("| Buttons:                               |*");
  puts("|   (1) Switch to finger control mode    |*");
  puts("|   (2) Switch to arm control mode       |*");
  puts("|   (3) Switch to base control mode      |*");
  puts("|   (4) Switch to sensor control mode    |*");
  puts(" ----------------------------------------**");
  puts("  *****************************************");
}

int main(int argc, char **argv)
{
  // initialize ROS and the node
  ros::init(argc, argv, "carl_joy_teleop");

  // initialize the joystick controller
  carl_joy_teleop controller;

  ros::Rate loop_rate(60);  //rate at which to publish arm velocity commands
  while (ros::ok())
  {
    controller.publish_velocity();
    ros::spinOnce();
    loop_rate.sleep();
  }

  return EXIT_SUCCESS;
}
