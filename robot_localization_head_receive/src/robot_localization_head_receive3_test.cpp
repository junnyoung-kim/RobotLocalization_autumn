#include <stdio.h>
#include <iostream>
#include <math.h>
#include <string>
#include <Eigen/Dense>
#include "ros/ros.h"

#include <yaml-cpp/yaml.h>
#include <ros/package.h>

#include <std_msgs/Float32MultiArray.h>
#include <sensor_msgs/JointState.h>
#include <robot_localization_msgs/HeadTransform.h>

using namespace ros;
using namespace std;
using namespace Eigen;

float angle_torso_pitch=0.0;
float angle_torso_yaw=0.0;
float angle_head_yaw=0.0;
float angle_head_pitch=0.0;
    
void jointCallback(const sensor_msgs::JointState::ConstPtr& msg);

bool RobotYamlRead(string path, float* length_pelvis2torsopitch, float* length_torsopitch2torsoyaw, float* length_torsoyaw2headyaw, float* length_headyaw2headpitch, float* length_headpitch2camera);
bool ParticleYamlRead(string path, float& unit);

Matrix4f makeYawMatrix(float& theta);
Matrix4f makePitchMatrix(float& theta);
Matrix4f makeTransMatrix(Vector3f& position);
Matrix4f makeTransMatrix(Vector3f& position, float& unit);

void printMatrixValue(Matrix4f& matrix);

void insertMatrix2Message(Matrix4f& matrix, std_msgs::Float32MultiArray& msg);
void insertMatrix2Message(Matrix4f& matrix, robot_localization_msgs::HeadTransform& msg, int flag); // flag=0 -> original, flag=1 -> unit

int main(int argc, char** argv)
{
    //read setting
    string yaml_robot_path = package::getPath("robot_localization_data") + "/data/kin_dyn_3.yaml";
    string yaml_particle_path = package::getPath("robot_localization_data") + "/data/particle_setting.yaml";
  
    float length_pelvis2torsopitch[3];
    float length_torsopitch2torsoyaw[3];

    float length_torsoyaw2headyaw[3];
    float length_headyaw2headpitch[3];
    float length_headpitch2camera[3];

    float unit;
   
    if( !RobotYamlRead(yaml_robot_path, length_pelvis2torsopitch, length_torsopitch2torsoyaw, length_torsoyaw2headyaw, length_headyaw2headpitch, length_headpitch2camera) ) exit(0);
    if( !ParticleYamlRead(yaml_particle_path, unit) ) exit(0);
   
    Vector3f V_length_pelvis2torsopitch(length_pelvis2torsopitch[0], length_pelvis2torsopitch[1], length_pelvis2torsopitch[2]);
    Vector3f V_length_torsopitch2torsoyaw(length_torsopitch2torsoyaw[0], length_torsopitch2torsoyaw[1], length_torsopitch2torsoyaw[2]);
    Vector3f V_length_torsoyaw2headyaw(length_torsoyaw2headyaw[0],length_torsoyaw2headyaw[1],length_torsoyaw2headyaw[2]);
    Vector3f V_length_headyaw2headpitch(length_headyaw2headpitch[0],length_headyaw2headpitch[1],length_headyaw2headpitch[2]);
    Vector3f V_length_headpitch2camera(length_headpitch2camera[0], length_headpitch2camera[1], length_headpitch2camera[2]);

    //start ros
    ros::init(argc, argv, "robot_localization_head_receive3");
    ros::NodeHandle n;
    ros::Rate loop_rate(100);
    
    ros::Publisher headtransform_pub = n.advertise<std_msgs::Float32MultiArray>("/alice/camera_transform",10);
    ros::Publisher custom_headtransform_pub = n.advertise<robot_localization_msgs::HeadTransform>("/robot_localization/head_transform",10);
    ros::Subscriber joint_sub = n.subscribe("/robotis/present_joint_states",10,jointCallback);

    while(ros::ok())
    {
        std_msgs::Float32MultiArray headtransform_msg;
        robot_localization_msgs::HeadTransform custom_headtransform_msg;

        //real world
        Matrix4f t_pelvis2torsopitch = makeTransMatrix(V_length_pelvis2torsopitch);
        Matrix4f r_pelvis2torsopitch = Matrix4f::Identity();

        Matrix4f t_torsopitch2torsoyaw = makeTransMatrix(V_length_torsopitch2torsoyaw);
        Matrix4f r_torsopitch2torsoyaw = makePitchMatrix(angle_torso_pitch);

        Matrix4f t_torsoyaw2headyaw = makeTransMatrix(V_length_torsoyaw2headyaw);
        Matrix4f r_torsoyaw2headyaw = makeYawMatrix(angle_torso_yaw);
        
        Matrix4f t_headyaw2headpitch = makeTransMatrix(V_length_headyaw2headpitch);
        Matrix4f r_headyaw2headpitch = makeYawMatrix(angle_head_yaw);

        Matrix4f t_headpitch2camera = makeTransMatrix(V_length_headpitch2camera);
        Matrix4f r_headpitch2camera = makePitchMatrix(angle_head_pitch);

        Matrix4f TR01 = r_pelvis2torsopitch * t_pelvis2torsopitch;
        Matrix4f TR12 = r_torsopitch2torsoyaw * t_torsopitch2torsoyaw;
        Matrix4f TR23 = r_torsoyaw2headyaw * t_torsoyaw2headyaw;
        Matrix4f TR34 = r_headyaw2headpitch * t_headyaw2headpitch;
        Matrix4f TR45 = r_headpitch2camera * t_headpitch2camera;

        Matrix4f Transform_pelvis2head = TR01 * TR12 * TR23 * TR34 * TR45;  // Transform matrix real world = TR

        //virtual world
        Matrix4f t_pelvis2torsopitch_V = makeTransMatrix(V_length_pelvis2torsopitch, unit);
        Matrix4f t_torsopitch2torsoyaw_V = makeTransMatrix(V_length_torsopitch2torsoyaw, unit);
        Matrix4f t_torsoyaw2headyaw_V = makeTransMatrix(V_length_torsoyaw2headyaw, unit);
        Matrix4f t_headyaw2headpitch_V = makeTransMatrix(V_length_headyaw2headpitch, unit);
        Matrix4f t_headpitch2camera_V = makeTransMatrix(V_length_headpitch2camera, unit);

        Matrix4f TV01 = r_pelvis2torsopitch * t_pelvis2torsopitch_V;
        Matrix4f TV12 = r_torsopitch2torsoyaw * t_torsopitch2torsoyaw_V;
        Matrix4f TV23 = r_torsoyaw2headyaw * t_torsoyaw2headyaw_V;
        Matrix4f TV34 = r_headyaw2headpitch * t_headyaw2headpitch_V;
        Matrix4f TV45 = r_headpitch2camera * t_headpitch2camera_V;

        Matrix4f unit_Transform_pelvis2head = TV01 * TV12 * TV23 * TV34 * TV45;   // Transform matrix virtual world = TV

        insertMatrix2Message(Transform_pelvis2head, headtransform_msg);

        insertMatrix2Message(Transform_pelvis2head, custom_headtransform_msg, 0);
        insertMatrix2Message(unit_Transform_pelvis2head, custom_headtransform_msg, 1);

        headtransform_pub.publish(headtransform_msg);
        custom_headtransform_pub.publish(custom_headtransform_msg);
      
        cout << "-----------------------------------------------" << endl;
        cout << "waist_pitch : " << angle_torso_pitch * 180 / M_PI << endl;
        cout << "waist_yaw : " << angle_torso_yaw * 180 / M_PI << endl;
        cout << "head_yaw : " << angle_head_yaw * 180 / M_PI << endl;
        cout << "head_pitch : " << angle_head_pitch * 180 / M_PI << endl;
        cout << "-----------------------------------------------" << endl;
        printMatrixValue(Transform_pelvis2head);
        cout << "-----------------------------------------------" << endl << endl;


        ros::spinOnce();
        loop_rate.sleep();
    }

    return 0;
}

void jointCallback(const sensor_msgs::JointState::ConstPtr& msg)
{
    int torso_pitch_index, torso_yaw_index, head_yaw_index, head_pitch_index;
    for(int i=0; i<(msg->name).size(); i++)
    {
        if(msg->name[i] == "waist_pitch") torso_pitch_index = i;
        if(msg->name[i] == "waist_yaw"  ) torso_yaw_index = i;
        if(msg->name[i] == "head_yaw"   ) head_yaw_index = i;
        if(msg->name[i] == "head_pitch" ) head_pitch_index = i;
    }
   
    angle_torso_pitch = msg->position[torso_pitch_index];
    angle_torso_yaw = msg->position[torso_yaw_index];
    angle_head_yaw = msg->position[head_yaw_index];
    angle_head_pitch = msg->position[head_pitch_index];
}

bool RobotYamlRead(string path, float* length_pelvis2torsopitch, float* length_torsopitch2torsoyaw, float* length_torsoyaw2headyaw, float* length_headyaw2headpitch, float* length_headpitch2camera)
{
    YAML::Node yaml_node;
    try
    {
        yaml_node = YAML::LoadFile(path.c_str());

        length_pelvis2torsopitch[0]   = yaml_node["torso_p"]["relative_position"][0].as<float>();
        length_pelvis2torsopitch[1]   = yaml_node["torso_p"]["relative_position"][1].as<float>();
        length_pelvis2torsopitch[2]   = yaml_node["torso_p"]["relative_position"][2].as<float>();

        length_torsopitch2torsoyaw[0] = yaml_node["torso_y"]["relative_position"][0].as<float>();
        length_torsopitch2torsoyaw[1] = yaml_node["torso_y"]["relative_position"][1].as<float>();
        length_torsopitch2torsoyaw[2] = yaml_node["torso_y"]["relative_position"][2].as<float>();

        length_torsoyaw2headyaw[0]    = yaml_node["head_y"]["relative_position"][0].as<float>();
        length_torsoyaw2headyaw[1]    = yaml_node["head_y"]["relative_position"][1].as<float>();
        length_torsoyaw2headyaw[2]    = yaml_node["head_y"]["relative_position"][2].as<float>();

        length_headyaw2headpitch[0]  = yaml_node["head_p"]["relative_position"][0].as<float>();
        length_headyaw2headpitch[1]  = yaml_node["head_p"]["relative_position"][1].as<float>();
        length_headyaw2headpitch[2]  = yaml_node["head_p"]["relative_position"][2].as<float>();

        length_headpitch2camera[0]   = yaml_node["cam"]["relative_position"][0].as<float>();
        length_headpitch2camera[1]   = yaml_node["cam"]["relative_position"][1].as<float>();
        length_headpitch2camera[2]   = yaml_node["cam"]["relative_position"][2].as<float>();
    }
    catch(const exception& e)
    {
        ROS_ERROR("fail to read robot yaml file");
        return false;
    }
    return true;
}

bool ParticleYamlRead(string path, float& unit)
{
    YAML::Node yaml_node;
    try
    {
        yaml_node = YAML::LoadFile(path.c_str());
        unit = yaml_node["ETCSetting"]["unit"].as<float>();
    }
    catch(const exception& e)
    {
        ROS_ERROR("fail to read particle yaml file");
        return false;
    }
    return true;
}

void insertMatrix2Message(Matrix4f& matrix, std_msgs::Float32MultiArray& msg)
{
    for(int i=0; i<4; i++)
        for(int j=0; j<4; j++)
            msg.data.push_back(matrix(i,j));
}

void printMatrixValue(Matrix4f& matrix){
    vector<float> data;
    for(int i=0; i<4; i++){
        for(int j=0; j<4; j++){
            data.push_back(matrix(i,j));
            printf("%3.3f  ", data.back());
        }
        cout << endl;
    }
}

void insertMatrix2Message(Matrix4f& matrix, robot_localization_msgs::HeadTransform& msg, int flag)
{
    switch(flag)
    {
        case 0:
            for(int i=0; i<4; i++)
                for(int j=0; j<4; j++)
                    msg.Tdata.push_back(matrix(i,j));
            break;
        case 1:
            for(int i=0; i<4; i++)
                for(int j=0; j<4; j++)
                    msg.Udata.push_back(matrix(i,j));
            break;
        default:
            break;
    }
}

Matrix4f makeYawMatrix(float& theta)
{
    Matrix4f matrix;
    matrix(0,0)=cos(theta); matrix(0,1)=-sin(theta); matrix(0,2)=0; matrix(0,3)=0;
    matrix(1,0)=sin(theta); matrix(1,1)= cos(theta); matrix(1,2)=0; matrix(1,3)=0;
    matrix(2,0)=0;          matrix(2,1)=0;           matrix(2,2)=1; matrix(2,3)=0;
    matrix(3,0)=0;          matrix(3,1)=0;           matrix(3,2)=0; matrix(3,3)=1;

    return matrix;
}

Matrix4f makePitchMatrix(float& theta)
{
    Matrix4f matrix;
    matrix(0,0)= cos(theta); matrix(0,1)=0; matrix(0,2)=sin(theta); matrix(0,3)=0;
    matrix(1,0)=0;           matrix(1,1)=1; matrix(1,2)=0;          matrix(1,3)=0;
    matrix(2,0)=-sin(theta); matrix(2,1)=0; matrix(2,2)=cos(theta); matrix(2,3)=0;
    matrix(3,0)=0;           matrix(3,1)=0; matrix(3,2)=0;          matrix(3,3)=1;

    return matrix;
}

Matrix4f makeTransMatrix(Vector3f& position)
{
    Matrix4f matrix;
    matrix(0,0)=1; matrix(0,1)=0; matrix(0,2)=0; matrix(0,3)=position(0);
    matrix(1,0)=0; matrix(1,1)=1; matrix(1,2)=0; matrix(1,3)=position(1);
    matrix(2,0)=0; matrix(2,1)=0; matrix(2,2)=1; matrix(2,3)=position(2);
    matrix(3,0)=0; matrix(3,1)=0; matrix(3,2)=0; matrix(3,3)=1;

    return matrix;
}

Matrix4f makeTransMatrix(Vector3f& position, float& unit)
{
    Matrix4f matrix;
    matrix(0,0)=1; matrix(0,1)=0; matrix(0,2)=0; matrix(0,3)=position(0)*unit;
    matrix(1,0)=0; matrix(1,1)=1; matrix(1,2)=0; matrix(1,3)=position(1)*unit;
    matrix(2,0)=0; matrix(2,1)=0; matrix(2,2)=1; matrix(2,3)=position(2)*unit;
    matrix(3,0)=0; matrix(3,1)=0; matrix(3,2)=0; matrix(3,3)=1;

    return matrix;
}
