#pragma once

#include "nav_msgs/Odometry.h"
#include "nuscenes2bag/Filesystem.hpp"
#include "nuscenes2bag/MetaDataTypes.hpp"

#include <cv_bridge/cv_bridge.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <boost/optional.hpp>

namespace nuscenes2bag {

boost::optional<nav_msgs::Odometry> readWheelFile(const WheelData& filePath) noexcept;

}
