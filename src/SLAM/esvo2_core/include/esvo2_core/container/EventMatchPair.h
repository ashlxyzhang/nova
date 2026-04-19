#ifndef ESVO2_CORE_CORE_EVENTMATCHPAIR_H
#define ESVO2_CORE_CORE_EVENTMATCHPAIR_H

#include <deque>
#include <esvo2_core/container/CameraSystem.h>
#include <esvo2_core/container/TimeSurfaceObservation.h>
#include <esvo2_core/tools/utils.h>
#include <vector>

namespace esvo2_core
{
using namespace container;
using namespace tools;
namespace core
{
struct EventMatchPair
{
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW

        EventMatchPair()
        {
        }

        // raw event coordinate
        Eigen::Vector2d x_left_raw_;
        // rectified_event coordinate (left, right)
        Eigen::Vector2d x_left_, x_right_;
        // timestamp
        timePoint t_;
        // pose of virtual view (T_world_virtual)
        Transformation trans_;
        // inverse depth
        double invDepth_;
        // match cost
        double cost_;
        // gradient (left)
        double gx_, gy_;
        // disparity
        double disp_;

        // for debug
        Eigen::Vector2d x_last_;
        double error_ = 10000;
        double lr_cost;
        double lr_depth;
        double ln_cost;
        double ln_depth;
        std::vector<double> costs_;
};
} // namespace core
} // namespace esvo2_core

#endif // ESVO2_CORE_CORE_EVENTMATCHPAIR_H
