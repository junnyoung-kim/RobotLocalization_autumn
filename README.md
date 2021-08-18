# RobotLocalization for autumn heroehscup 
use known keypoints monte carlo localzation algorithm

### Requirements
```
git clone https://github.com/RobotWebTools/mjpeg_server
cm
```

### How to use
```
## Detection 
roslaunch zed_wrapper zed.launch
roslaunch darknet_ros darknet_ros.launch
rosrun alice_vision alice_vision_node

## Robot Localization
rosrun robot_localization_pf_landmarks robot_localization_pf_landmarks

## Initialize start/restart postion
rostopic pub /alice/reposition geometry_msgs/Pose2D \
"x: 5.2 / 2.8 / 5.8
y: 3.7 / 0.7
theta: 0.0 / 1.57" 

## Web streaming
rosrun mjpeg_server mjpeg_server _port:=8080
http://[network ip]:8080/stream?topic=//robot_localization/localization_image

## or just Launch parameterized launch file 
roslaunch robot_localization_pf_landmarks robot_localization.launch mode:=kinematics/PF gui_mode:=on/off
```

### Data Record
```
rosbag record -o filename.bag /alice/ideal_body_delta /alice/vision/detected_objects /alice/moving_status /robotis/present_joint_states /rosout /rosout_agg
```
