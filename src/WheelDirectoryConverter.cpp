#include "nuscenes2bag/WheelDirectoryConverter.hpp"
#include "nuscenes2bag/utils.hpp"
#include <thread>
#define pi 3.14159265358979323846
#define radius 0.305 // Known Zoe wheel radius in meters.
#define steer_radio 16.6// Zoe steer radio
#define wheel_base 2.588 // Zoe wheel base

namespace nuscenes2bag {

    boost::optional<nav_msgs::Odometry>
    readWheelFile(const WheelData &wheelData) noexcept {
        try {
            nav_msgs::Odometry msg;
            msg.header = std_msgs::Header();
            auto RL_wheel_speed = wheelData.RL_wheel_speed;
            auto RR_wheel_speed = wheelData.RR_wheel_speed;
            auto steer_corrected = wheelData.steer_corrected;

            // 将速度转成m/s
            double circumference = 2 * pi * radius;
            RL_wheel_speed *= circumference / 60;
            RR_wheel_speed *= circumference / 60;
            double vx = 0.5*(RL_wheel_speed + RR_wheel_speed);

            // 将转角转成弧度
            steer_corrected = steer_corrected * pi / (180 * steer_radio);
            double twist_z =  vx * tan(steer_corrected) / wheel_base;

//            msg.header.stamp = ros::Time().fromSec(data.header.stamp.sec + data.header.stamp.nsec / 1e9);
            msg.twist.twist.linear.x = vx;
            msg.twist.twist.linear.y = 0;
            msg.twist.twist.linear.z = 0;
            msg.twist.twist.angular.x = 0;
            msg.twist.twist.angular.y = 0;
            msg.twist.twist.angular.z = twist_z;
            return boost::optional<nav_msgs::Odometry>(msg);

        } catch (const std::exception &e) {
            PRINT_EXCEPTION(e);
        }
        return boost::none;
    }

}