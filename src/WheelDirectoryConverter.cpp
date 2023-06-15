#include "nuscenes2bag/WheelDirectoryConverter.hpp"
#include "nuscenes2bag/utils.hpp"
#include <thread>
#define pi 3.14159265358979323846

namespace nuscenes2bag {

    boost::optional<nav_msgs::Odometry>
    readWheelFile(const WheelData &wheelData) noexcept {
        try {
            nav_msgs::Odometry msg;
            msg.header = std_msgs::Header();
            auto RL_wheel_speed = wheelData.RL_wheel_speed;
            auto RR_wheel_speed = wheelData.RR_wheel_speed;

            // 将速度转成m/s
            double radius = 0.305; // Known Zoe wheel radius in meters.
            double circumference = 2 * pi * radius;
            RL_wheel_speed *= circumference / 60;
            RR_wheel_speed *= circumference / 60;
//            msg.header.stamp = ros::Time().fromSec(data.header.stamp.sec + data.header.stamp.nsec / 1e9);
            msg.twist.twist.linear.x = 0.5*(RL_wheel_speed + RR_wheel_speed);
            msg.twist.twist.linear.y = 0;
            msg.twist.twist.linear.z = 0;
            msg.twist.twist.angular.x = 0;
            msg.twist.twist.angular.y = 0;
            msg.twist.twist.angular.z = 0;
            return boost::optional<nav_msgs::Odometry>(msg);

        } catch (const std::exception &e) {
            PRINT_EXCEPTION(e);
        }

        return boost::none;
    }

}