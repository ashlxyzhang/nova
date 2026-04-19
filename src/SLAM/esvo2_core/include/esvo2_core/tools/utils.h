#ifndef ESVO2_CORE_TOOLS_UTILS_H
#define ESVO2_CORE_TOOLS_UTILS_H

#include <Eigen/Eigen>
#include <iostream>

#include <kindr/minimal/quat-transformation.h>

#include "src/SLAM/esvo2_core/include/esvo2_core/tools/types.h"

#include <opencv2/core/eigen.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <esvo2_core/container/DepthPoint.h>
#include <esvo2_core/container/SmartGrid.h>
#include <esvo2_core/tools/TicToc.h>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <sys/stat.h>

namespace esvo2_core
{
namespace tools
{
// TUNE this according to your platform's computational capability.
#define NUM_THREAD_TRACKING 1
#define NUM_THREAD_MAPPING 4
using timePoint = std::chrono::time_point<std::chrono::steady_clock>;

using PointCloud = pcl::PointCloud<pcl::PointXYZ>;
using RefPointCloudMap = std::map<timePoint, PointCloud::Ptr>;
using Transformation = kindr::minimal::QuatTransformation;
using StampTransformationMap = std::map<timePoint, Transformation>;

using EventQueue = std::deque<esvo2_core::Event>;

inline EventQueue::iterator EventBuffer_lower_bound(EventQueue &eb, timePoint &t)
{
    return std::lower_bound(eb.begin(), eb.end(), t,
                            [](const esvo2_core::Event &e, const timePoint &t) { return e.timestamp < t; });
}

inline EventQueue::iterator EventBuffer_upper_bound(EventQueue &eb, timePoint &t)
{
    return std::upper_bound(eb.begin(), eb.end(), t,
                            [](const timePoint &t, const esvo2_core::Event &e) { return t < e.timestamp; });
}

/******************* Used by Block Match ********************/
inline void meanStdDev(Eigen::MatrixXd &patch, double &mean, double &sigma)
{
    double numElement = (patch.rows() * patch.cols());
    mean = patch.array().sum() / numElement;
    Eigen::MatrixXd sub = patch.array() - mean;
    sigma = std::sqrt((sub.array() * sub.array()).sum() / numElement) + 1e-6;
}

inline void normalizePatch(Eigen::MatrixXd &patch_src, Eigen::MatrixXd &patch_dst)
{
    double mean = 0;
    double sigma = 0;
    meanStdDev(patch_src, mean, sigma);
    sigma = 1.0 / sigma;
    patch_dst = (patch_src.array() - mean) * sigma;
}

// recursively create a directory
// inline void _mkdir(const char *dir)
// {
//     char tmp[256];
//     char *p = NULL;
//     size_t len;

//     snprintf(tmp, sizeof(tmp), "%s", dir);
//     len = strlen(tmp);
//     if (tmp[len - 1] == '/')
//         tmp[len - 1] = 0;
//     for (p = tmp + 1; *p; p++)
//         if (*p == '/')
//         {
//             *p = 0;
//             mkdir(tmp, S_IRWXU);
//             *p = '/';
//         }
//     mkdir(tmp, S_IRWXU);
// }

} // namespace tools
} // namespace esvo2_core
#endif // ESVO2_CORE_TOOLS_UTILS_H
