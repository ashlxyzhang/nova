#ifndef ESVO2_CORE_CORE_BACKEND_H
#define ESVO2_CORE_CORE_BACKEND_H

// #include <esvo2_core/esvo2_Mapping.h>
#include "esvo2_core/container/CameraSystem.h"
#include "esvo2_core/container/DepthMap.h"
#include "esvo2_core/container/DepthPoint.h"
#include "esvo2_core/container/TimeSurfaceObservation.h"
#include "esvo2_core/factor/OptimizationFunctor.h"
// #include <esvo2_core/factor/imu_factor.h>
// #include <esvo2_core/factor/pose_local_parameterization.h>
#include <esvo2_core/tools/Visualization.h>
#include <esvo2_core/tools/utils.h>
#include <esvo2_core/factor/imu_integration.h>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/imgproc.hpp>
#include "esvo2_core/tools/types.h"
#include <Eigen/Dense>
#include <chrono>

#include "data_passing.hh"

namespace esvo2_core
{
using namespace container;
using namespace tools;
using namespace factor;
namespace core
{

    const int WINDOW_SIZE = 4;
    struct DepthPointFrame
    {
        using timePoint = std::chrono::time_point<std::chrono::steady_clock>;
        timePoint timestamp_;
        std::vector<DepthPoint> DepthPoints_;

        DepthPointFrame(timePoint t, std::vector<DepthPoint> DepthPoints)
        {
            timestamp_ = t;
            DepthPoints_ = DepthPoints;
        }

        int size()
        {
            return DepthPoints_.size();
        }
    };

class BackendOptimization
{
    public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW

        BackendOptimization(const CameraSystem::Ptr &camSysPtr, DataPassingDeque<esvo2_core::VBaBg>& v_ba_bg_Map_to_Track);

        void setProblem(std::deque<DepthPointFrame> *dqvDepthPoints, TimeSurfaceHistory *pTS_history, bool bUSE_IMU);
        void sloveProblem();

        void double2Vector(double para_Pose[][7], double para_SpeedBias[][9]);
        void Tcam2Timu(esvo2_core::container::TimeSurfaceHistory::iterator ts_obs,
                       esvo2_core::container::TimeSurfaceHistory::iterator last_obs, double para_Pose[][7], int i);
        void vector2Double(esvo2_core::container::TimeSurfaceHistory::iterator ts_obs,
                           esvo2_core::container::TimeSurfaceHistory::iterator last_obs, double para_Pose[][7],
                           double para_SpeedBias[][9], int i);
        void publishVBaBg(double time_v);
        bool CalibrationExRotation(Eigen::Matrix3d &calib_ric_result);
        bool LinearAlignment(Eigen::Vector3d &g, Eigen::VectorXd &x);
        void solveGyroscopeBias(Eigen::Vector3d &Bgs);
        void RefineGravity(Eigen::Vector3d &g, Eigen::VectorXd &x);
        void slideWindow();
        bool isOrthogonal(const Eigen::Matrix3d &matrix);
        Eigen::Matrix3d fixRotationMatrix(const Eigen::Matrix3d &R);
        // bool getPoseAt(const timePoint &t, esvo2_core::Transformation &Tr, const std::string& source_frame);

    private:
        // Queue
        DataPassingDeque<esvo2_core::VBaBg>& v_ba_bg_Map_to_Track_;

        CameraSystem::Ptr camSysPtr_;
        std::deque<DepthPointFrame> *pDepthPoints_;
        TimeSurfaceHistory *pTS_history_;
        bool initVsFlag;
        bool bUSE_IMU_;

        Eigen::Vector3d TIC_ = Eigen::Vector3d::Zero();
        Eigen::Matrix3d RIC_;
        Eigen::Matrix4d T_i_c_;

    public:
        int frame_count;
        Eigen::Vector3d g_optimal;
        Eigen::Vector3d Ps[(WINDOW_SIZE + 1)];
        Eigen::Vector3d Ps_before[(WINDOW_SIZE + 1)];
        Eigen::Vector3d Vs[(WINDOW_SIZE + 1)];
        Eigen::Matrix3d Rs[(WINDOW_SIZE + 1)];
        Eigen::Vector3d Bas[(WINDOW_SIZE + 1)];
        Eigen::Vector3d Bgs[(WINDOW_SIZE + 1)];

        IntegrationBase *pre_integrations[(WINDOW_SIZE + 1)];
        Eigen::Vector3d acc_0, gyr_0;
        std::vector<double> time_of_pts_;
        Eigen::Matrix3d K_, K_of_pts_, K_pts_inv_;
        Eigen::Matrix4d T_rect_raw_;
        const std::string world_frame_id_ = "world";
        const std::string dvs_frame_id_ = "dvs";
        Eigen::Matrix4d T_wopt_window0_;
};
} // namespace core
} // namespace esvo2_core

#endif // ESVO2_CORE_CORE_DEPTHPROBLEM_H
