#ifndef ESVO2_CORE_MAPPING_H
#define ESVO2_CORE_MAPPING_H

// From STD library
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <string>

// From SLAM
#include <esvo2_core/tools/types.h>
#include <esvo2_core/container/CameraSystem.h>
#include <esvo2_core/container/DepthMap.h>
#include <esvo2_core/container/EventMatchPair.h>
#include <esvo2_core/core/DepthFusion.h>
#include <esvo2_core/core/DepthProblem.h>
#include <esvo2_core/core/DepthProblemSolver.h>
#include <esvo2_core/core/DepthRegularization.h>
#include <esvo2_core/core/EventBM.h>
#include <esvo2_core/tools/Visualization.h>
#include <esvo2_core/tools/utils.h>
#include <esvo2_core/core/BackendOptimization.h>
#include <esvo2_core/factor/imu_integration.h>
#include <data_passing.hh>
#include <multi_data_passing.hh>

// From dependencies
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <pcl/point_types.h>
#include <yaml-cpp/yaml.h>
#include <Eigen/Dense>

namespace esvo2_core
{
using timePoint = std::chrono::time_point<std::chrono::steady_clock>;
using EventQueue = std::deque<esvo2_core::Event>;
using namespace core;

class esvo2_Mapping
{
    public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
        esvo2_Mapping(std::atomic<bool> &is_running_, const YAML::Node &config, 
            const std::string& left_camera_yaml_path, const std::string& right_camera_yaml_path,
            DataPassingDeque<esvo2_core::VBaBg>& v_ba_bg_Map_to_Track,
            DataPassingDeque<pcl::PointCloud<pcl::PointXYZRGBL>>& pointcloud_Map_to_Track
        );
        virtual ~esvo2_Mapping();

        // mapping
        void MappingLoop(std::promise<void> prom_mapping, std::future<void> future_reset);
        void MappingAtTime(const timePoint &t);
        bool InitializationAtTime(const timePoint &t);
        bool dataTransferring();

        // callback functions
        void stampedPoseCallback(const std::shared_ptr<esvo2_core::PoseStamped> &ps_msg);
        void eventsCallback(const std::shared_ptr<esvo2_core::EventArray> &msg);
        void timeSurfaceCallback(const esvo2_core::ImagePtr &time_surface_left,
                                 const esvo2_core::ImagePtr &time_surface_right,
                                 const esvo2_core::ImagePtr &AA_map,
                                 const esvo2_core::ImagePtr &time_surface_negative,
                                 const esvo2_core::ImagePtr &time_surface_dx,
                                 const esvo2_core::ImagePtr &time_surface_dy);
        void AACallback(const esvo2_core::ImagePtr &AA_left);
        void refImuCallback(const std::shared_ptr<esvo2_core::ImuMsg> &msg);
        // utils
        bool getPoseAt(const timePoint &t, Transformation &Tr, const std::string &source_frame);
        void clearEventQueue(EventQueue &EQ);
        void reset();

        /*** publish results ***/
        void publishMappingResults(DepthMap::Ptr depthMapPtr, Transformation tr, timePoint t, cv::Mat invDepthImage);
        void publishPointCloud(DepthMap::Ptr &depthMapPtr, Transformation &tr, timePoint &t);
        void publishImage(const cv::Mat &image, const timePoint &t, std::string encoding = "bgr8");

        std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBL>> get_viz_pointcloud()
        {
            std::lock_guard<std::mutex> lock(viz_pc_mutex_);
            return viz_pc_;
        }

        std::shared_ptr<pcl::PointCloud<pcl::PointXYZ>> get_viz_global_pointcloud()
        {
            std::lock_guard<std::mutex> lock(viz_pc_mutex_);
            return viz_pc_global_;
        }

        bool were_pointclouds_updated()
        {
            bool answer = pointclouds_updated;
            pointclouds_updated = false;
            return answer;
        }

        /*** event processing ***/
        void createEdgeMask(std::vector<esvo2_core::Event *> &vEventsPtr, PerspectiveCamera::Ptr &camPtr, cv::Mat &edgeMap,
                            std::vector<std::pair<std::size_t, std::size_t>> &vEdgeletCoordinates, bool bUndistortEvents = true,
                            std::size_t radius = 0);

        void createDenoisingMask(std::vector<esvo2_core::Event *> &vAllEventsPtr, cv::Mat &mask, std::size_t row,
                                 std::size_t col); // reserve in this file

        void extractDenoisedEvents(std::vector<esvo2_core::Event *> &vCloseEventsPtr, std::vector<esvo2_core::Event *> &vEdgeEventsPtr,
                                   cv::Mat &mask, std::size_t maxNum = 5000);

        void getReprojection(std::vector<EventMatchPair> &vEMP, Eigen::Matrix4d T_last_now,
                             std::vector<esvo2_core::Event *> &vDenoisedEventsPtr_left_dy_);
        void selectPoint();
        bool getIMUInterval(double t0, double t1, vector<pair<double, Eigen::Vector3d>> &accVector,
                            vector<pair<double, Eigen::Vector3d>> &gyrVector);
        void initFirstIMUPose(vector<pair<double, Eigen::Vector3d>> &accVector);
        void processIMU(double t, double dt, const Eigen::Vector3d &linear_acceleration,
                        const Eigen::Vector3d &angular_velocity);

        /************************ member variables ************************/
        bool bpoints_from_AA_; // must be public so slam_manager can access it
    private:
        //queues
        // DataPassingDeque<esvo2_core::VBaBg>& v_ba_bg_Map_to_Track; //MOVED TO BACKEND_OPTIMIZATION.h
        DataPassingDeque<pcl::PointCloud<pcl::PointXYZRGBL>>& pointcloud_Map_to_Track_;

        // visualization
        std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBL>> viz_pc_;
        std::mutex viz_pc_mutex_;
        std::shared_ptr<pcl::PointCloud<pcl::PointXYZ>> viz_pc_global_;
        std::atomic<bool> pointclouds_updated = false;

        // Running variable
        std::atomic<bool> &is_running;

        // configuration variables struct
        YAML::Node config_;

        double t_last_pub_pc_;

        // offline data
        std::string dvs_frame_id_;
        std::string world_frame_id_;
        CameraSystem::Ptr camSysPtr_;

        // imu data
        double prevTime;
        bool first_imu;

        // online data
        EventQueue events_left_, events_right_;
        TimeSurfaceHistory TS_history_;
        constStampedTimeSurfaceObs *TS_obs_ptr_;
        std::set<timePoint> currProcessingTSTimes;
        StampTransformationMap st_map_;
        std::shared_ptr<esvo2_core::Transformer> tf_;
        std::size_t TS_id_;
        timePoint tf_lastest_common_time_;

        // system
        DepthProblemConfig::Ptr dpConfigPtr_, dpConfigPtr_ln_;
        DepthProblemSolver dpSolver_, dpSolver_ln_;
        DepthFusion dFusor_, dFusor_ln_;
        DepthRegularization dRegularizor_, dRegularizor_ln_;
        Visualization visualizor_;
        EventBM ebm_;

        // data transfer
        std::vector<esvo2_core::Event *> vALLEventsPtr_left_;          // for BM
        std::vector<esvo2_core::Event *> vCloseEventsPtr_left_;        // for BM
        std::vector<esvo2_core::Event *> vDenoisedEventsPtr_left_;     // for BM
        std::vector<esvo2_core::Event *> vDenoisedEventsPtr_left_dx_;  // for BM
        std::vector<esvo2_core::Event *> vDenoisedEventsPtr_left_dx2_; // for BM
        std::vector<esvo2_core::Event *> vDenoisedEventsPtr_left_dy_;  // for BM
        std::size_t totalNumCount_;                             // count the number of events involved
        std::vector<esvo2_core::Event *> vEventsPtr_left_SGM_;         // for SGM

        // result
        PointCloud::Ptr pc_near_, pc_global_;
        pcl::PointCloud<pcl::PointXYZRGBL>::Ptr pc_color_, pc_filtered_;
        DepthFrame::Ptr depthFramePtr_;
        bool blarge_scale_;

        std::deque<DepthPointFrame> dqvDepthPoints_, dqvDepthPoints_ln_;
        // DepthPointFrame

        // inter-thread management
        std::mutex data_mutex_;
        std::promise<void> mapping_thread_promise_, reset_promise_;
        std::future<void> mapping_thread_future_, reset_future_;

        /**** mapping parameters ***/
        // range and visualization threshold
        double invDepth_min_range_;
        double invDepth_max_range_;
        double cost_vis_threshold_, cost_vis_threshold_ln_;
        std::size_t patch_area_;
        double residual_vis_threshold_, residual_vis_threshold_ln_;
        double stdVar_vis_threshold_, stdVar_vis_threshold_ln_;
        std::size_t age_max_range_;
        std::size_t age_vis_threshold_;
        int fusion_radius_;
        std::string FusionStrategy_;
        int maxNumFusionFrames_, maxNumFusionFrames_ln_;
        int maxNumFusionPoints_;
        std::size_t INIT_SGM_DP_NUM_Threshold_;
        // module parameters
        std::size_t PROCESS_EVENT_NUM_;
        std::size_t PROCESS_EVENT_NUM_AA_;
        std::size_t TS_HISTORY_LENGTH_;
        std::size_t mapping_rate_hz_;
        // options
        bool changed_frame_rate_;
        bool bRegularization_;
        bool resetButton_;
        bool bDenoising_;
        bool bVisualizeGlobalPC_;
        // visualization parameters
        double visualizeGPC_interval_;
        double visualize_range_;
        std::size_t numAddedPC_threshold_;
        // Event Block Matching (BM) parameters
        double BM_half_slice_thickness_, eta_for_select_points_;
        std::size_t BM_patch_size_X_, BM_patch_size_X_2_;
        std::size_t BM_patch_size_Y_, BM_patch_size_Y_2_;
        std::size_t BM_min_disparity_;
        std::size_t BM_max_disparity_;
        std::size_t BM_step_;
        double BM_ZNCC_Threshold_;
        bool BM_bUpDownConfiguration_;
        bool bUSE_IMU_;
        // Select points from AA
        int x_patches_, y_patches_;

        double distance_from_last_frame_;

        // SGM parameters (Used by Initialization)
        int num_disparities_;
        int block_size_;
        int P1_;
        int P2_;
        int uniqueness_ratio_;
        cv::Ptr<cv::StereoSGBM> sgbm_;

        BackendOptimization BackendOpt_;

        queue<pair<double, Eigen::Vector3d>> accBuf;
        queue<pair<double, Eigen::Vector3d>> gyrBuf;
        bool initFirstPoseFlag;
        Eigen::Vector3d acc_0, gyr_0;
        std::mutex mBuf;

        /**********************************************************/
        /******************** For test & debug ********************/
        /**********************************************************/
        std::string resultPath_;
        // For counting the total number of fusion
        std::size_t TotalNumFusion_;
        double data_trans_time;
};
} // namespace esvo2_core

#endif // ESVO2_CORE_MAPPING_H
