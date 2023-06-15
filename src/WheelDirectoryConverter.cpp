#include "nuscenes2bag/WheelDirectoryConverter.hpp"
#include "nuscenes2bag/utils.hpp"
#include <thread>

namespace nuscenes2bag {

    boost::optional<nav_msgs::Odometry>
    readWheelFile(const WheelData &wheelData) noexcept {
        try {
            nav_msgs::Odometry msg;
            msg.header = std_msgs::Header();
//            msg.header.stamp = ros::Time().fromSec(data.header.stamp.sec + data.header.stamp.nsec / 1e9);
            msg.twist.twist.linear.x = 0.5*(wheelData.RL_wheel_speed + wheelData.RR_wheel_speed);
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