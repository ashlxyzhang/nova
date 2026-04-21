#include <esvo2_core/esvo2_Mapping.h>
#include <esvo2_core/factor/pose_local_parameterization.h>
#include <esvo2_core/factor/utility.h>
// #include <minkindr_conversions/kindr_tf.h>

// #include <geometry_msgs/TransformStamped.h>

#include <opencv2/imgproc.hpp>

#include <pcl/filters/voxel_grid.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <thread>
#include <utility>
#include <string>

#include <data_passing.hh>
#include <multi_data_passing.hh>
#include <esvo2_core/tools/utils.h>
#include <esvo2_core/tools/types.h>

// #define ESVO2_CORE_MAPPING_DEBUG
// #define ESVO2_CORE_MAPPING_LOG

namespace esvo2_core
{
esvo2_Mapping::esvo2_Mapping(std::atomic<bool> &is_running_, const YAML::Node &config, 
            const std::string& left_camera_yaml_path, const std::string& right_camera_yaml_path,
            DataPassingDeque<esvo2_core::VBaBg>& v_ba_bg_Map_to_Track,
            DataPassingDeque<pcl::PointCloud<pcl::PointXYZRGBL>>& pointcloud_Map_to_Track)
    : is_running(is_running_), config_(config), pointcloud_Map_to_Track_(pointcloud_Map_to_Track), 
    // calibInfoDir_(config_["calibInfoDir"].as<std::string>("")),
      camSysPtr_(new CameraSystem(left_camera_yaml_path, right_camera_yaml_path, false)),
      dpConfigPtr_(new DepthProblemConfig(
          config_["patch_size_X"].as<int>(5), config_["patch_size_Y"].as<int>(5),
          config_["LSnorm_ln"].as<std::string>("Tdist"), config_["Tdist_nu"].as<double>(0.0),
          config_["Tdist_scale"].as<double>(0.0), config_["ITERATION_OPTIMIZATION"].as<int>(1),
          config_["RegularizationRadius"].as<int>(5), config_["RegularizationMinNeighbours"].as<int>(8),
          config_["RegularizationMinCloseNeighbours"].as<int>(8))),
      dpSolver_(camSysPtr_, dpConfigPtr_, NUMERICAL, NUM_THREAD_MAPPING, true), dFusor_(camSysPtr_, dpConfigPtr_),
      dRegularizor_(dpConfigPtr_),
      dpConfigPtr_ln_(new DepthProblemConfig(
          config_["patch_size_X"].as<int>(25), config_["patch_size_Y"].as<int>(25),
          config_["LSnorm_ln"].as<std::string>("Tdist"), config_["Tdist_nu_ln"].as<double>(0.0),
          config_["Tdist_scale_ln"].as<double>(0.0), config_["ITERATION_OPTIMIZATION_LN"].as<int>(1),
          config_["RegularizationRadius"].as<int>(5), config_["RegularizationMinNeighbours"].as<int>(8),
          config_["RegularizationMinCloseNeighbours"].as<int>(8))),
      dpSolver_ln_(camSysPtr_, dpConfigPtr_ln_, NUMERICAL, NUM_THREAD_MAPPING, false),
      dFusor_ln_(camSysPtr_, dpConfigPtr_ln_), dRegularizor_ln_(dpConfigPtr_ln_),
      ebm_(camSysPtr_, NUM_THREAD_MAPPING, config_["SmoothTimeSurface"].as<bool>(false)), pc_near_(new PointCloud()),
      pc_global_(new PointCloud()),
      depthFramePtr_(new DepthFrame(camSysPtr_->cam_left_ptr_->height_, camSysPtr_->cam_left_ptr_->width_)),
      BackendOpt_(camSysPtr_, v_ba_bg_Map_to_Track)
{
    // frame id
    dvs_frame_id_ = config_["dvs_frame_id"].as<std::string>("dvs");
    world_frame_id_ = config_["world_frame_id"].as<std::string>("world");
    pc_near_->header.frame_id = world_frame_id_;
    pc_global_->header.frame_id = world_frame_id_;
    pc_color_ = pcl::PointCloud<pcl::PointXYZRGBL>::Ptr(new pcl::PointCloud<pcl::PointXYZRGBL>());
    pc_color_->header.frame_id = world_frame_id_;
    pc_filtered_ = pcl::PointCloud<pcl::PointXYZRGBL>::Ptr(new pcl::PointCloud<pcl::PointXYZRGBL>());
    pc_filtered_->header.frame_id = world_frame_id_;

    /**** mapping parameters ***/
    // range and visualization threshold
    invDepth_min_range_ = config_["invDepth_min_range"].as<double>(0.16);
    invDepth_max_range_ = config_["invDepth_max_range"].as<double>(2.0);
    patch_area_ = config_["patch_size_X"].as<int>(25) * config_["patch_size_Y"].as<int>(25);
    residual_vis_threshold_ = config_["residual_vis_threshold"].as<int>(15);
    residual_vis_threshold_ln_ = config_["residual_vis_threshold_ln"].as<int>(15);
    cost_vis_threshold_ = pow(residual_vis_threshold_, 2) * patch_area_;
    cost_vis_threshold_ln_ = pow(residual_vis_threshold_ln_, 2) * patch_area_;
    stdVar_vis_threshold_ = config_["stdVar_vis_threshold"].as<double>(0.005);
    stdVar_vis_threshold_ln_ = config_["stdVar_vis_threshold_ln"].as<int>(1);
    age_max_range_ = config_["age_max_range"].as<int>(5);
    age_vis_threshold_ = config_["age_vis_threshold"].as<int>(0);
    fusion_radius_ = config_["fusion_radius"].as<int>(0);
    maxNumFusionFrames_ = config_["maxNumFusionFrames"].as<int>(10);
    maxNumFusionFrames_ln_ = config_["maxNumFusionFrames_ln"].as<int>(10);
    FusionStrategy_ = config_["FUSION_STRATEGY"].as<std::string>("CONST_FRAMES");
    maxNumFusionPoints_ = config_["maxNumFusionPoints"].as<int>(2000);
    INIT_SGM_DP_NUM_Threshold_ = config_["INIT_SGM_DP_NUM_THRESHOLD"].as<std::size_t>(500);

    // options
    bDenoising_ = config_["Denoising"].as<bool>(false);
    bRegularization_ = config_["Regularization"].as<bool>(false);
    resetButton_ = config_["ResetButton"].as<bool>(false);
    blarge_scale_ = config_["large_scale"].as<bool>(true);
    bpoints_from_AA_ = config_["select_points_from_AA"].as<bool>(true);
    eta_for_select_points_ = config_["eta_for_select_points"].as<double>(0.1);

    // visualization parameters
    bVisualizeGlobalPC_ = config_["bVisualizeGlobalPC"].as<bool>(false);
    visualizeGPC_interval_ = config_["visualizeGPC_interval"].as<int>(3);
    visualize_range_ = config_["visualize_range"].as<double>(2.5);
    numAddedPC_threshold_ = config_["NumGPC_added_per_refresh"].as<int>(1000);

    // module parameters
    PROCESS_EVENT_NUM_ = config_["PROCESS_EVENT_NUM"].as<int>(500);
    PROCESS_EVENT_NUM_AA_ = config_["PROCESS_EVENT_NUM_AA"].as<int>(500);
    TS_HISTORY_LENGTH_ = config_["TS_HISTORY_LENGTH"].as<int>(100);
    mapping_rate_hz_ = config_["mapping_rate_hz"].as<int>(20);

    // Event Block Matching (BM) parameters
    BM_half_slice_thickness_ = config_["BM_half_slice_thickness"].as<double>(0.001);
    BM_patch_size_X_ = config_["patch_size_X"].as<int>(25);
    BM_patch_size_Y_ = config_["patch_size_Y"].as<int>(25);
    BM_patch_size_X_2_ = config_["patch_size_X_2"].as<int>(25);
    BM_patch_size_Y_2_ = config_["patch_size_Y_2"].as<int>(25);
    x_patches_ = config_["x_patches"].as<int>(8);
    y_patches_ = config_["y_patches"].as<int>(6);
    BM_min_disparity_ = config_["BM_min_disparity"].as<int>(3);
    BM_max_disparity_ = config_["BM_max_disparity"].as<int>(40);
    BM_step_ = config_["BM_step"].as<int>(1);
    BM_ZNCC_Threshold_ = config_["BM_ZNCC_Threshold"].as<double>(0.1);
    BM_bUpDownConfiguration_ = config_["BM_bUpDownConfiguration"].as<bool>(false);
    bUSE_IMU_ = config_["USE_IMU"].as<bool>(true);

    // distance from last frame
    distance_from_last_frame_ = config_["distance_from_last_frame"].as<double>(0.04);

    // SGM parameters (Used by Initialization)
    num_disparities_ = BM_max_disparity_;
    block_size_ = 11;
    P1_ = 8 * 1 * block_size_ * block_size_;
    P2_ = 32 * 1 * block_size_ * block_size_;
    uniqueness_ratio_ = 11;
    sgbm_ = cv::StereoSGBM::create(0, num_disparities_, block_size_, P1_, P2_, -1, 0, uniqueness_ratio_);

    // calcualte the min,max disparity of static block matching
    double f = (camSysPtr_->cam_left_ptr_->P_(0, 0) + camSysPtr_->cam_left_ptr_->P_(1, 1)) / 2;
    double b = camSysPtr_->baseline_;
    std::size_t minDisparity = std::max(std::size_t(std::floor(f * b * invDepth_min_range_)), (std::size_t)0);
    std::size_t maxDisparity = std::size_t(std::ceil(f * b * invDepth_max_range_));
    minDisparity = std::max(minDisparity, BM_min_disparity_);
    maxDisparity = std::min(maxDisparity, BM_max_disparity_);

    // Backend parameters
    initFirstPoseFlag = false;
    prevTime = 0; //\Users\jackm\Desktop\nova\vcpkg\buildtrees\openeb\src\5.2.0-b808f57c67.clean\sdk\modules\stream\cpp\samples\metavision_file_to_dat\build\build>
    first_imu = false;

    // initialize Event Batch Matcher
    ebm_.resetParameters(BM_patch_size_X_, BM_patch_size_Y_, minDisparity, maxDisparity, BM_step_, BM_ZNCC_Threshold_,
                         BM_bUpDownConfiguration_, BM_patch_size_X_2_, BM_patch_size_Y_2_);
    BM_min_disparity_ = minDisparity;
    BM_max_disparity_ = maxDisparity;
    // system status
    setSystemStatus(SystemStatus::INITIALIZATION);

    // callback functions
    // stampedPose_sub_ = nh_.subscribe("stamped_pose", 0, &esvo2_Mapping::stampedPoseCallback, this);
    // TS_AA_sync_.registerCallback(boost::bind(&esvo2_Mapping::timeSurfaceCallback, this, _1, _2, _3, _4, _5, _6));

    // point sampling
    // if (bpoints_from_AA_)
    //     // AA_frequency_sub_ = nh_.subscribe<sensor_msgs::Image>("AA_left", 0, &esvo2_Mapping::AACallback, this);
    // else
    //     events_left_sub_ = nh_.subscribe<EventArray>(
    //         "events_left", 0, boost::bind(&esvo2_Mapping::eventsCallback, this, _1, boost::ref(events_left_)));

    // IMU
    // if (bUSE_IMU_)
        // imu_sub_ = nh_.subscribe("/imu/data", 0, &esvo2_Mapping::refImuCallback, this);

    // TF
    tf_ = std::make_shared<esvo2_core::Transformer>(100);

    // result publishers
    // In refactoring, below two have no SUB/PUB queues set up!
    // invDepthMap_pub_ = it_.advertise("Inverse_Depth_Map2", 1);
    // pc_filtered_pub_ = nh_.advertise<PointCloud>("/esvo2_mapping/pointcloud_filtered2", 1);
    if (bVisualizeGlobalPC_)
    {
        // gpc_pub_ = nh_.advertise<PointCloud>("/esvo2_mapping/pointcloud_global2", 1);
        pc_global_->reserve(5000000);
        t_last_pub_pc_ = 0.0;
    }

    // multi-thread management
    mapping_thread_future_ = mapping_thread_promise_.get_future();
    reset_future_ = reset_promise_.get_future();

    // stereo mapping detached thread
    std::thread MappingThread(&esvo2_Mapping::MappingLoop, this, std::move(mapping_thread_promise_),
                              std::move(reset_future_));
    MappingThread.detach();

    // The onlineParameterChangeCallback is empty, so I am pretty sure this does nothing.
    // I also commented out the onlineParameterChangeCallback function.
    // Dynamic reconfigure.
    // dynamic_reconfigure_callback_ = boost::bind(&esvo2_Mapping::onlineParameterChangeCallback, this, _1, _2);

    // server_.reset(new dynamic_reconfigure::Server<DVS_MappingStereoConfig>(nh_private));
    // server_->setCallback(dynamic_reconfigure_callback_);
}

esvo2_Mapping::~esvo2_Mapping()
{
    // pc_pub_.shutdown();
    // pc_filtered_pub_.shutdown();
    // invDepthMap_pub_.shutdown();
    // V_ba_bg_pub_.shutdown();
}

void esvo2_Mapping::MappingLoop(std::promise<void> prom_mapping, std::future<void> future_reset)
{
    // ros::Rate r(mapping_rate_hz_);
    // while (ros::ok())
    // {
    //     // reset mapping rate
    //     if (changed_frame_rate_)
    //     {
    //         r = ros::Rate(mapping_rate_hz_);
    //         changed_frame_rate_ = false;
    //     }

    //     // check system status
    //     if (getSystemStatus() == SystemStatus::TERMINATE)
    //     {
    //         std::cout << "The Mapping node is terminated manually...";
    //         break;
    //     }

    //     // To assure the esvo2_time_surface node has been working
    //     if (TS_history_.size() >= 10)
    //     {
    //         TicToc total_mapping;
    //         while (true)
    //         {
    //             if (data_mutex_.try_lock())
    //             {
    //                 dataTransferring();
    //                 data_mutex_.unlock();
    //                 break;
    //             }
    //             else
    //             {
    //                 if (future_reset.wait_for(std::chrono::nanoseconds(1)) == std::future_status::ready)
    //                 {
    //                     prom_mapping.set_value();
    //                     return;
    //                 }
    //             }
    //         }

    //         // To check if the most current TS observation has been loaded by dataTransferring()
    //         if (TS_obs_ptr_->second.isEmpty())
    //         {
    //             r.sleep();
    //             continue;
    //         }

    //         // Do initialization (State Machine)
    //         if (getSystemStatus() == SystemStatus::INITIALIZATION || getSystemStatus() == SystemStatus::RESET)
    //         {
    //             if (InitializationAtTime(TS_obs_ptr_->first))
    //             {
    //                 std::cout << "Initialization is successfully done!"; //(" << INITIALIZATION_COUNTER_ << ").";
    //             }
    //             else
    //                 std::cout << "Initialization fails once.";
    //         }
    //         double Data_transfer = total_mapping.toc();

    //         // Do mapping
    //         if (getSystemStatus() == SystemStatus::WORKING)
    //             MappingAtTime(TS_obs_ptr_->first);

    //         BackendOpt_.slideWindow();
    //     }
    //     else
    //     {
    //         if (future_reset.wait_for(std::chrono::nanoseconds(1)) == std::future_status::ready)
    //         {
    //             prom_mapping.set_value();
    //             return;
    //         }
    //     }
    //     r.sleep();
    // }

    std::chrono::nanoseconds interval = std::chrono::nanoseconds(static_cast<long long>(1e9/mapping_rate_hz_));
    timePoint next_wake_up_time = std::chrono::steady_clock::now();
    while (is_running)
    {
        // reset mapping rate
        if (changed_frame_rate_)
        {
            interval = std::chrono::nanoseconds(static_cast<long long>(1e9/mapping_rate_hz_));
            changed_frame_rate_ = false;
        }

        // check system status
        if (getSystemStatus() == SystemStatus::TERMINATE)
        {
            std::cout << "The Mapping node is terminated manually..." << std::endl;
            break;
        }

        // To assure the esvo2_time_surface node has been working
        if (TS_history_.size() >= 10)
        {
            TicToc total_mapping;
            while (true)
            {
                if (data_mutex_.try_lock())
                {
                    dataTransferring();
                    data_mutex_.unlock();
                    break;
                }
                else
                {
                    if (future_reset.wait_for(std::chrono::nanoseconds(1)) == std::future_status::ready)
                    {
                        prom_mapping.set_value();
                        return;
                    }
                }
            }

            // To check if the most current TS observation has been loaded by dataTransferring()
            if (TS_obs_ptr_->second.isEmpty())
            {
                next_wake_up_time += interval;
                std::this_thread::sleep_until(next_wake_up_time);
                continue;
            }

            // Do initialization (State Machine)
            if (getSystemStatus() == SystemStatus::INITIALIZATION || getSystemStatus() == SystemStatus::RESET)
            {
                if (InitializationAtTime(TS_obs_ptr_->first))
                {
                    std::cout << "Initialization is successfully done!"<<std::endl;
                }
                else
                    std::cout << "Initialization fails once."<<std::endl;
            }
            double Data_transfer = total_mapping.toc();

            // Do mapping
            if (getSystemStatus() == SystemStatus::WORKING)
                MappingAtTime(TS_obs_ptr_->first);

            BackendOpt_.slideWindow();
        }
        else
        {
            if (future_reset.wait_for(std::chrono::nanoseconds(1)) == std::future_status::ready)
            {
                prom_mapping.set_value();
                return;
            }
        }
        next_wake_up_time += interval;
        std::this_thread::sleep_until(next_wake_up_time);
    }
}

void esvo2_Mapping::MappingAtTime(const timePoint &t)
{
    TicToc tt_mapping;
    TicToc mapping_cost; // record the time cost of each step
    double t_overall_count = 0;
    /************************************************/
    /************ set the new DepthFrame ************/
    /************************************************/
    DepthFrame::Ptr depthFramePtr_new =
        std::make_shared<DepthFrame>(camSysPtr_->cam_left_ptr_->height_, camSysPtr_->cam_left_ptr_->width_);
    depthFramePtr_new->setId(TS_obs_ptr_->second.id_);
    depthFramePtr_new->setTransformation(TS_obs_ptr_->second.tr_);
    depthFramePtr_ = depthFramePtr_new;
    std::vector<EventMatchPair> vEMP; // the container that stores the result of BM.
    /****************************************************/
    /*************** Block Matching (BM) ****************/
    /****************************************************/
    double t_BM = 0.0;
    double t_BM_denoising = 0.0;
    cv::Mat denoising_mask;

    vDenoisedEventsPtr_left_.clear();
    vDenoisedEventsPtr_left_.reserve(PROCESS_EVENT_NUM_);
    vDenoisedEventsPtr_left_.insert(vDenoisedEventsPtr_left_.end(), vCloseEventsPtr_left_.begin(),
                                    vCloseEventsPtr_left_.begin() +
                                        min(vCloseEventsPtr_left_.size(), PROCESS_EVENT_NUM_));

    totalNumCount_ = vDenoisedEventsPtr_left_.size();
    t_BM_denoising = tt_mapping.toc();
    t_overall_count += t_BM_denoising;
    tt_mapping.tic();
    double t_BM_select;
    // divide the events into two parts according to gradient.
    selectPoint();
    t_BM_select = tt_mapping.toc();
    t_overall_count += t_BM_select;

    // Denoising operations, only for static stereo matching
    if (bDenoising_)
    {
        tt_mapping.tic();
        // Draw one mask image for denoising.
        createDenoisingMask(vALLEventsPtr_left_, denoising_mask, camSysPtr_->cam_left_ptr_->height_,
                            camSysPtr_->cam_left_ptr_->width_);
        vDenoisedEventsPtr_left_dx2_.clear();
        // Extract denoised events (appear on edges likely).
        extractDenoisedEvents(vDenoisedEventsPtr_left_dx_, vDenoisedEventsPtr_left_dx2_, denoising_mask,
                              PROCESS_EVENT_NUM_);
        t_BM_denoising += tt_mapping.toc();
    }
    else
    {
        vDenoisedEventsPtr_left_dx2_.clear();
        vDenoisedEventsPtr_left_dx2_.insert(vDenoisedEventsPtr_left_dx2_.end(), vDenoisedEventsPtr_left_dx_.begin(),
                                            vDenoisedEventsPtr_left_dx_.begin() +
                                                min(vDenoisedEventsPtr_left_dx_.size(), PROCESS_EVENT_NUM_));
    }
    t_overall_count += t_BM_denoising;
    tt_mapping.tic();

    // static stereo matching
    ebm_.createMatchProblem(TS_obs_ptr_, &st_map_, &vDenoisedEventsPtr_left_dx2_);
    ebm_.match_all_HyperThread(vEMP);

    std::vector<EventMatchPair> vEMP_last_pre, vEMP_last, vEMP_last_fail;
    vEMP_last_pre.reserve(vDenoisedEventsPtr_left_dy_.size());
    vEMP_last.reserve(vDenoisedEventsPtr_left_dy_.size());
    vEMP_last_fail.reserve(vDenoisedEventsPtr_left_dy_.size());

    // determine whether the last frame is available
    if (!(TS_obs_ptr_->second.TS_last_.rows() == 0 || TS_obs_ptr_->second.TS_last_.cols() == 0 ||
          (TS_obs_ptr_->second.tr_last_.getPosition() - TS_obs_ptr_->second.tr_.getPosition()).norm() <
              distance_from_last_frame_))
    {
        Eigen::Matrix4d T_last_now = TS_obs_ptr_->second.tr_last_.getTransformationMatrix().inverse() *
                                     TS_obs_ptr_->second.tr_.getTransformationMatrix();

        // get the direction of epipolar line
        getReprojection(vEMP_last_pre, T_last_now, vDenoisedEventsPtr_left_dy_);
        cv::cv2eigen(TS_obs_ptr_->second.img_AA_map_.clone(), TS_obs_ptr_->second.AA_map_);
        cv::cv2eigen(TS_obs_ptr_->second.img_last_.clone(), TS_obs_ptr_->second.TS_last_);

        // temporal stereo matching
        ebm_.createMatchProblemTwoFrames(TS_obs_ptr_, &st_map_, &vDenoisedEventsPtr_left_dy_, &vEMP_last_pre);
        ebm_.match_all_HyperThreadTwoFrames(vEMP_last, vEMP_last_fail);
    }

    t_BM = tt_mapping.toc();
    t_overall_count += t_BM_denoising;
    t_overall_count += t_BM;

    /**************************************************************/
    /*************  Fusion ***************/
    /**************************************************************/
    double t_optimization = 0;
    double t_solve, t_fusion, t_regularization;
    t_solve = t_fusion = t_regularization = 0;
    std::size_t numFusionCount = 0, numFusionCount_ln = 0; // To count the total number of fusion (in terms of fusion between
                                                      // two estimates, i.e. a priori and a propagated one).
    tt_mapping.tic();

    // just compute the variance of the residual
    std::vector<DepthPoint> vdp, vdp_ln;
    vdp.reserve(vEMP.size() + vEMP_last.size());
    dpSolver_.solve(&vEMP, TS_obs_ptr_, vdp);
    vdp_ln.reserve(vEMP_last.size());
    dpSolver_ln_.solve(&vEMP_last, TS_obs_ptr_, vdp_ln);

    dpSolver_.pointCulling(vdp, stdVar_vis_threshold_, cost_vis_threshold_, invDepth_min_range_, invDepth_max_range_);
    if (blarge_scale_)
        dpSolver_ln_.pointCulling(vdp_ln, stdVar_vis_threshold_ln_, cost_vis_threshold_ln_, invDepth_min_range_,
                                  invDepth_max_range_);

    t_solve = tt_mapping.toc();
    tt_mapping.tic();

    if (FusionStrategy_ == "CONST_POINTS") // Fusion (strategy 1: const number of point)
    {
        std::size_t numFusionPoints = 0;
        DepthPointFrame vdpf(t, vdp);
        dqvDepthPoints_.push_back(vdpf);
        for (std::size_t n = 0; n < dqvDepthPoints_.size(); n++)
            numFusionPoints += dqvDepthPoints_[n].size();
        while (numFusionPoints > 1.5 * maxNumFusionPoints_)
        {
            dqvDepthPoints_.pop_front();
            numFusionPoints = 0;
            for (std::size_t n = 0; n < dqvDepthPoints_.size(); n++)
                numFusionPoints += dqvDepthPoints_[n].size();
        }
    }
    else if (FusionStrategy_ == "CONST_FRAMES") // (strategy 2: const number of frames)
    {
        DepthPointFrame vdpf(t, vdp), vdpf_ln(t, vdp_ln);
        dqvDepthPoints_.push_back(vdpf);
        dqvDepthPoints_ln_.push_back(vdpf_ln);
        while (dqvDepthPoints_.size() > maxNumFusionFrames_)
            dqvDepthPoints_.pop_front();
        while (dqvDepthPoints_ln_.size() > maxNumFusionFrames_ln_)
            dqvDepthPoints_ln_.pop_front();
    }
    else
        std::cout << "Invalid FusionStrategy is assigned.";

    // apply fusion and count the total number of fusion.
    numFusionCount = 0;
    int total = 0;
    for (auto it = dqvDepthPoints_.rbegin(); it != dqvDepthPoints_.rend(); it++)
    {
        total += it->size();
        numFusionCount += dFusor_.update(it->DepthPoints_, depthFramePtr_, fusion_radius_);
        if (blarge_scale_)
            for (int i = 0; i < 3; i++)
            {
                if (it != dqvDepthPoints_.rend() - 1)
                    it++;
            }
    }
    TotalNumFusion_ += numFusionCount + numFusionCount_ln;
    if (dqvDepthPoints_.size() >= maxNumFusionFrames_)
        depthFramePtr_->dMap_->clean(pow(stdVar_vis_threshold_, 2), age_vis_threshold_, invDepth_max_range_,
                                     invDepth_min_range_);

    double data_time = tt_mapping.toc();

    // regularization
    if (bRegularization_)
    {
        tt_mapping.tic();
        dRegularizor_.apply(depthFramePtr_->dMap_);
        t_regularization = tt_mapping.toc();
    }
    tt_mapping.tic();
    for (auto it = dqvDepthPoints_ln_.rbegin(); it != dqvDepthPoints_ln_.rend(); it++)
    {
        total += it->size();
        numFusionCount_ln += dFusor_ln_.update(it->DepthPoints_, depthFramePtr_, fusion_radius_);
    }

    // count time
    t_fusion = tt_mapping.toc() + data_time;
    t_optimization = t_solve + t_fusion + t_regularization;
    t_overall_count += t_optimization;

    // publish results
    TicToc t_optimize;
    if (dqvDepthPoints_.size() >= WINDOW_SIZE + 1 && bUSE_IMU_ == true)
    {
        BackendOpt_.setProblem(&dqvDepthPoints_, &TS_history_, bUSE_IMU_);
        BackendOpt_.sloveProblem();
    }
    double time_optimize = t_optimize.toc();
    t_overall_count += time_optimize;

    std::thread tPublishMappingResult(&esvo2_Mapping::publishMappingResults, this, depthFramePtr_->dMap_,
                                      depthFramePtr_->T_world_frame_, t);
    tPublishMappingResult.detach();
#ifdef ESVO2_CORE_MAPPING_LOG
    std::cout << "\n";
    std::cout << "------------------------------------------------------------";
    std::cout << "--------------Computation Cost (Mapping)---------------------";
    std::cout << "------------------------------------------------------------";
    std::cout << "Denoising: " << t_BM_denoising << " ms, (" << t_BM_denoising / t_overall_count * 100 << "%).";
    std::cout << "Point Selection (BM): " << t_BM_select << " ms, (" << t_BM_select / t_overall_count * 100 << "%).";
    std::cout << "Block Matching (BM): " << t_BM << " ms, (" << t_BM / t_overall_count * 100 << "%).";
    std::cout << "BM success ratio: " << vEMP.size() + vEMP_last.size() << "/" << totalNumCount_
              << "(Successes/Total).";
    std::cout << "------------------------------------------------------------";
    std::cout << "------------------------------------------------------------";
    std::cout << "Update: " << t_optimization << " ms, (" << t_optimization / t_overall_count * 100 << "%).";
    std::cout << "-- compute variance: " << t_solve << " ms, (" << t_solve / t_overall_count * 100 << "%).";
    std::cout << "-- fusion (" << numFusionCount << ", " << TotalNumFusion_ << "): " << t_fusion << " ms, ("
              << t_fusion / t_overall_count * 100 << "%).";
    std::cout << "-- regularization: " << t_regularization << " ms, (" << t_regularization / t_overall_count * 100
              << "%).";
    std::cout << "-- time_optimize: " << time_optimize << " ms, (" << time_optimize / t_overall_count * 100 << "%).";
    std::cout << "------------------------------------------------------------";
    std::cout << "------------------------------------------------------------";
    std::cout << "Total Computation (" << depthFramePtr_->dMap_->size() << "): " << t_overall_count << " ms. ----"
              << mapping_cost.toc() << " ms";
    std::cout << "------------------------------------------------------------";
    std::cout << "------------------------------END---------------------------";
    std::cout << "------------------------------------------------------------";
    std::cout << "\n";
#endif
}

bool esvo2_Mapping::InitializationAtTime(const timePoint &t)
{
    // create a new depth frame
    DepthFrame::Ptr depthFramePtr_new =
        std::make_shared<DepthFrame>(camSysPtr_->cam_left_ptr_->height_, camSysPtr_->cam_left_ptr_->width_);
    depthFramePtr_new->setId(TS_obs_ptr_->second.id_);
    depthFramePtr_new->setTransformation(TS_obs_ptr_->second.tr_);
    depthFramePtr_ = depthFramePtr_new;

    // call SGM on the current Time Surface observation pair.
    cv::Mat dispMap, dispMap8;
    sgbm_->compute(TS_obs_ptr_->second.img_left_, TS_obs_ptr_->second.img_right_, dispMap);
    dispMap.convertTo(dispMap8, CV_8U, 255 / (num_disparities_ * 16.));

    // get the event map (binary mask)
    cv::Mat edgeMap;
    std::vector<std::pair<std::size_t, std::size_t>> vEdgeletCoordinates;
    createEdgeMask(vEventsPtr_left_SGM_, camSysPtr_->cam_left_ptr_, edgeMap, vEdgeletCoordinates, true, 0);

    // Apply logical "AND" operation and transfer "disparity" to "invDepth".
    std::vector<DepthPoint> vdp_sgm;
    vdp_sgm.reserve(vEdgeletCoordinates.size());
    double var_SGM = pow(0.001, 2);
    for (std::size_t i = 0; i < vEdgeletCoordinates.size(); i++)
    {
        std::size_t x = vEdgeletCoordinates[i].first;
        std::size_t y = vEdgeletCoordinates[i].second;

        double disp = dispMap.at<short>(y, x) / 16.0;
        if (disp < 0)
            continue;
        DepthPoint dp(x, y);
        Eigen::Vector2d p_img(x * 1.0, y * 1.0);
        dp.update_x(p_img);
        double invDepth = disp / (camSysPtr_->cam_left_ptr_->P_(0, 0) * camSysPtr_->baseline_);
        if (invDepth < invDepth_min_range_ || invDepth > invDepth_max_range_)
            continue;
        Eigen::Vector3d p_cam;
        camSysPtr_->cam_left_ptr_->cam2World(p_img, invDepth, p_cam);
        dp.update_p_cam(p_cam);
        dp.update(invDepth, var_SGM); // assume the statics of the SGM's results are Guassian.
        dp.residual() = 0.0;
        dp.age() = age_vis_threshold_;
        Eigen::Matrix<double, 4, 4> T_world_cam = TS_obs_ptr_->second.tr_.getTransformationMatrix();
        dp.updatePose(T_world_cam);
        vdp_sgm.push_back(dp);
    }
    std::cout << vEventsPtr_left_SGM_.size() << "********** Initialization (SGM) returns " << vdp_sgm.size()
              << " points.";
    if (vdp_sgm.size() < INIT_SGM_DP_NUM_Threshold_)
        return false;
    // push the "masked" SGM results to the depthFrame
    DepthPointFrame vdpf_sgm(t, vdp_sgm);
    dqvDepthPoints_.push_back(vdpf_sgm);
    dFusor_.naive_propagation(vdp_sgm, depthFramePtr_);
    // publish the invDepth map
    std::thread tPublishMappingResult(&esvo2_Mapping::publishMappingResults, this, depthFramePtr_->dMap_,
                                      depthFramePtr_->T_world_frame_, t);
    tPublishMappingResult.detach();
    return true;
}

bool esvo2_Mapping::dataTransferring()
{
    TS_obs_ptr_ = NULL; // clean the TS obs.
    constStampedTimeSurfaceObs emptyObs;
    TS_obs_ptr_ = reinterpret_cast<constStampedTimeSurfaceObs *>(&emptyObs);

    // To assure the esvo2_time_surface node has been working.
    if (TS_history_.size() <= 10)
        return false;
    totalNumCount_ = 0;

    // load current Time-Surface Observation
    auto it_end = TS_history_.rbegin();
    it_end++; // in case that the tf is behind the most current TS.
    auto it_begin = TS_history_.begin();
    while (TS_obs_ptr_->second.isEmpty())
    {
        Transformation tr, tr_last;
        if (getSystemStatus() == SystemStatus::INITIALIZATION)
        {
            tr.setIdentity();
            it_end->second.setTransformation(tr);
            it_end->second.setOriTransformation(tr);
            TS_obs_ptr_ = &(*it_end);
        }
        if (getSystemStatus() == SystemStatus::WORKING)
        {
            if (getPoseAt(it_end->first, tr, dvs_frame_id_))
            {
                it_end->second.setTransformation(tr);
                it_end->second.setOriTransformation(tr);
                TS_obs_ptr_ = &(*it_end);
                while (it_end->first != it_begin->first)
                {
                    if (getPoseAt(it_end->first, tr_last, dvs_frame_id_))
                    {
                        if ((tr_last.getPosition() - tr.getPosition()).norm() > distance_from_last_frame_)
                        {
                            it_end->second.setTransformation(tr_last);
                            if (!it_end->second.isEmpty())
                            {
                                TS_obs_ptr_->second.tr_last_ = it_end->second.tr_;
                                TS_obs_ptr_->second.TS_last_ = it_end->second.AA_map_;
                                TS_obs_ptr_->second.TS_last_du = it_end->second.dTS_du_left_;
                                TS_obs_ptr_->second.TS_last_dv = it_end->second.dTS_dv_left_;
                                TS_obs_ptr_->second.img_last_ = it_end->second.img_AA_map_;
                            }

                            break;
                        }
                    }
                    it_end++;
                }
            }
            else
            {
                // check if the tracking node is still working normally
                if (getSystemStatus() != SystemStatus::WORKING)
                    return false;
            }
        }
        if (it_end->first == it_begin->first)
            break;
        it_end++;
    }
    if (TS_obs_ptr_->second.isEmpty())
        return false;

    std::vector<pair<double, Eigen::Vector3d>> accVector, gyrVector;
    double curTime = esvo2_core::timePointToSec(TS_obs_ptr_->first);
    if (prevTime == 0)
        prevTime = esvo2_core::timePointToSec(TS_obs_ptr_->first) - 0.5;
    mBuf.lock();
    // get the IMU data by time interval
    getIMUInterval(prevTime, curTime, accVector, gyrVector);
    mBuf.unlock();
    if (!initFirstPoseFlag)
        initFirstIMUPose(accVector);
    for (int i = 0; i < accVector.size(); i++)
    {
        double dt;
        if (i == 0)
            dt = accVector[i].first - prevTime;
        else if (i == accVector.size() - 1)
            dt = curTime - accVector[i - 1].first;
        else
            dt = accVector[i].first - accVector[i - 1].first;

        // imu pre-integration
        processIMU(accVector[i].first, dt, accVector[i].second, gyrVector[i].second);
    }
    prevTime = curTime;

    /****** Load involved events *****/
    // SGM
    if (getSystemStatus() == SystemStatus::INITIALIZATION)
    {
        vEventsPtr_left_SGM_.clear();
        timePoint t_end, t_begin;
        if (bpoints_from_AA_)
        {
            t_end = esvo2_core::secondsToTimePoint(esvo2_core::timePointToSec(TS_obs_ptr_->first) + 0.005);
            t_begin = esvo2_core::secondsToTimePoint(esvo2_core::timePointToSec(TS_obs_ptr_->first) - 0.005);
        }
        else
        {
            t_end = TS_obs_ptr_->first;
            t_begin = esvo2_core::secondsToTimePoint(std::max(0.0, esvo2_core::timePointToSec(t_end) - 10 * BM_half_slice_thickness_));
        }
        auto ev_end_it = tools::EventBuffer_lower_bound(events_left_, t_end);
        auto ev_begin_it = tools::EventBuffer_lower_bound(events_left_, t_begin);
        const std::size_t MAX_NUM_Event_INVOLVED = 30000;
        vEventsPtr_left_SGM_.reserve(MAX_NUM_Event_INVOLVED);
        while (ev_begin_it != ev_end_it && vEventsPtr_left_SGM_.size() <= PROCESS_EVENT_NUM_)
        {
            vEventsPtr_left_SGM_.push_back(&(*ev_begin_it));
            ev_begin_it++;
        }
    }

    // BM
    if (getSystemStatus() == SystemStatus::WORKING)
    {
        // copy all involved events' pointers
        vALLEventsPtr_left_
            .clear(); // Used to generate denoising mask (only used to deal with flicker induced by VICON.)
        vCloseEventsPtr_left_.clear(); // Will be denoised using the mask above.

        // load allEvent
        timePoint t_end, t_begin;
        if (bpoints_from_AA_)
        {
            t_end = esvo2_core::secondsToTimePoint(esvo2_core::timePointToSec(TS_obs_ptr_->first) + 0.005);
            t_begin = esvo2_core::secondsToTimePoint(esvo2_core::timePointToSec(TS_obs_ptr_->first) - 0.005);
        }
        else
        {
            t_end = TS_obs_ptr_->first;
            t_begin = esvo2_core::secondsToTimePoint(std::max(0.0, esvo2_core::timePointToSec(t_end) - 10 * BM_half_slice_thickness_));
        }
        auto ev_end_it = tools::EventBuffer_lower_bound(events_left_, t_end);
        auto ev_begin_it = tools::EventBuffer_lower_bound(events_left_, t_begin);
        const std::size_t MAX_NUM_Event_INVOLVED = PROCESS_EVENT_NUM_;
        vALLEventsPtr_left_.reserve(MAX_NUM_Event_INVOLVED);
        vCloseEventsPtr_left_.reserve(MAX_NUM_Event_INVOLVED);
        while (ev_end_it != ev_begin_it && vALLEventsPtr_left_.size() < MAX_NUM_Event_INVOLVED)
        {
            vALLEventsPtr_left_.push_back(&(*ev_end_it));
            vCloseEventsPtr_left_.push_back(&(*ev_end_it));
            ev_end_it--;
        }
        totalNumCount_ = vCloseEventsPtr_left_.size();
#ifdef ESVO2_CORE_MAPPING_DEBUG
        std::cout << "Data Transferring (events_left_): " << events_left_.size();
        std::cout << "Data Transferring (vALLEventsPtr_left_): " << vALLEventsPtr_left_.size();
        std::cout << "Data Transforming (vCloseEventsPtr_left_): " << vCloseEventsPtr_left_.size();
#endif
        if (vCloseEventsPtr_left_.size() < 100)
        {
            return false;
        }

#ifdef ESVO2_CORE_MAPPING_DEBUG
        std::cout << "Data Transferring (stampTransformation map): " << st_map_.size();
#endif
    }
    return true;
}

void esvo2_Mapping::stampedPoseCallback(const std::shared_ptr<esvo2_core::PoseStamped> &ps_msg)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    // To check inconsistent timestamps and reset.
    static constexpr double max_time_diff_before_reset_s = 0.5;
    const timePoint stamp_first_event = ps_msg->timestamp;
    std::string *err_tf = new std::string();
    delete err_tf;

    if (esvo2_core::timePointToSec(tf_lastest_common_time_) != 0)
    {
        const double dt = esvo2_core::timePointToSec(stamp_first_event) - esvo2_core::timePointToSec(tf_lastest_common_time_);
        if (dt < 0 || std::fabs(dt) >= max_time_diff_before_reset_s)
        {
            printf("Inconsistent event timestamps detected <stampedPoseCallback> (new: %f, old %f), resetting. \n",
                     esvo2_core::timePointToSec(stamp_first_event), esvo2_core::timePointToSec(tf_lastest_common_time_));
            reset();
        }
    }

    // add pose to tf
    esvo2_core::Transform tf(ps_msg->orientation[0], ps_msg->orientation[1], ps_msg->orientation[2],
                     ps_msg->orientation[3], ps_msg->position[0], ps_msg->position[1], ps_msg->position[2]);
    esvo2_core::StampedTransform st(tf, ps_msg->timestamp, ps_msg->frame_id, dvs_frame_id_);
    tf_->setTransform(st);
}

// return the pose of the left event cam at time t.
bool esvo2_Mapping::getPoseAt(const timePoint &t,
                              Transformation &Tr, // T_world_virtual
                              const std::string &source_frame)
{
    std::string *err_msg = new std::string();
    if (!tf_->canTransform(world_frame_id_, source_frame, t, err_msg))
    {
#ifdef ESVO2_CORE_MAPPING_LOG
        std::cout<<"WARNING:" << t.toNSec() << " : " << *err_msg<<std::endl;
#endif
        delete err_msg;
        return false;
    }
    else
    {
        esvo2_core::StampedTransform st;
        tf_->lookupTransform(world_frame_id_, source_frame, t, st);
        st.toKindrTransformation(Tr);
        // tf::transformTFToKindr(st, &Tr);
        return true;
    }
}

void esvo2_Mapping::eventsCallback(const std::shared_ptr<esvo2_core::EventArray> &msg)
{
    EventQueue &EQ = events_left_;
    std::lock_guard<std::mutex> lock(data_mutex_);

    static constexpr double max_time_diff_before_reset_s = 0.5;
    const timePoint stamp_first_event = msg->events[0].timestamp;

    // check timestamp consistency
    if (!msg->events.empty() && !EQ.empty())
    {
        const double dt = esvo2_core::timePointToSec(stamp_first_event) - esvo2_core::timePointToSec(EQ.back().timestamp);
        if (dt < 0 || std::fabs(dt) >= max_time_diff_before_reset_s)
        {
            printf("Inconsistent event timestamps detected <eventCallback> (new: %f, old %f), resetting.",
                     esvo2_core::timePointToSec(stamp_first_event), esvo2_core::timePointToSec(events_left_.back().timestamp));
            reset();
        }
    }

    // add new ones and remove old ones
    for (const Event &e : msg->events)
    {
        Eigen::Vector2d x;
        x << e.x, e.y;
        if (x(1) - 1 < 0 || x(1) + 1 >= camSysPtr_->cam_left_ptr_->height_ || x(0) - 1 < 0 ||
            x(0) + 1 >= camSysPtr_->cam_left_ptr_->width_) //||TS_gaussian.at<uchar>(x(1),x(0))<5)
            continue;
        EQ.push_back(e);
        int i = EQ.size() - 2;
        while (i >= 0 && EQ[i].timestamp > e.timestamp) // we may have to sort the queue, just in case the raw event messages do not
                                          // come in a chronological order.
        {
            EQ[i + 1] = EQ[i];
            i--;
        }
        EQ[i + 1] = e;
    }
    clearEventQueue(EQ);
}

void esvo2_Mapping::clearEventQueue(EventQueue &EQ)
{
    static constexpr std::size_t MAX_EVENT_QUEUE_LENGTH = 3000000;
    if (EQ.size() > MAX_EVENT_QUEUE_LENGTH)
    {
        std::size_t NUM_EVENTS_TO_REMOVE = EQ.size() - MAX_EVENT_QUEUE_LENGTH;
        EQ.erase(EQ.begin(), EQ.begin() + NUM_EVENTS_TO_REMOVE);
    }
}

void esvo2_Mapping::timeSurfaceCallback(const esvo2_core::ImagePtr &time_surface_left,
                                        const esvo2_core::ImagePtr &time_surface_right,
                                        const esvo2_core::ImagePtr &AA_map,
                                        const esvo2_core::ImagePtr &time_surface_negative,
                                        const esvo2_core::ImagePtr &time_surface_negative_dx,
                                        const esvo2_core::ImagePtr &time_surface_negative_dy)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    // check time-stamp inconsistency
    if (!TS_history_.empty())
    {
        static constexpr double max_time_diff_before_reset_s = 1;
        const timePoint stamp_last_image = TS_history_.rbegin()->first;
        const double dt = esvo2_core::timePointToSec(time_surface_left.header_stamp) - esvo2_core::timePointToSec(stamp_last_image);
        if (dt < 0 || std::fabs(dt) >= max_time_diff_before_reset_s)
        {
            printf("Inconsistent frame timestamp detected <timeSurfaceCallback> (new: %f, old %f), resetting.",
                     esvo2_core::timePointToSec(time_surface_left.header_stamp), esvo2_core::timePointToSec(stamp_last_image));
            reset();
        }
    }
    cv::Mat cv_ptr_left, cv_ptr_right, cv_ptr_AA_map_left, cv_ptr_negative, cv_ptr_negative_dx, cv_ptr_negative_dy;
    cv_ptr_left = *(time_surface_left.image); //cv_bridge::toCvCopy(time_surface_left, sensor_msgs::image_encodings::MONO8);
    cv_ptr_right = *(time_surface_right.image); //cv_bridge::toCvCopy(time_surface_right, sensor_msgs::image_encodings::MONO8);
    cv_ptr_AA_map_left = *(AA_map.image); //cv_bridge::toCvCopy(AA_map, sensor_msgs::image_encodings::MONO8);
    cv_ptr_negative = *(time_surface_negative.image); //cv_bridge::toCvCopy(time_surface_negative, sensor_msgs::image_encodings::MONO8);
    cv_ptr_negative_dx = *(time_surface_negative_dx.image); //cv_bridge::toCvCopy(time_surface_negative_dx, sensor_msgs::image_encodings::TYPE_16SC1);
    cv_ptr_negative_dy = *(time_surface_negative_dy.image); //cv_bridge::toCvCopy(time_surface_negative_dy, sensor_msgs::image_encodings::TYPE_16SC1);
   
    // push back the new time surface map
    timePoint t_new_TS = time_surface_left.header_stamp;

    // Made the gradient computation optional which is up to the jacobian choice.
    if (dpSolver_.getProblemType() == NUMERICAL || dpSolver_ln_.getProblemType() == NUMERICAL)
        TS_history_.emplace(t_new_TS,
                            TimeSurfaceObservation(cv_ptr_left, cv_ptr_right, cv_ptr_AA_map_left, cv_ptr_negative,
                                                   cv_ptr_negative_dx, cv_ptr_negative_dy, TS_id_, false));
    else
        TS_history_.emplace(t_new_TS,
                            TimeSurfaceObservation(cv_ptr_left, cv_ptr_right, cv_ptr_AA_map_left, cv_ptr_negative,
                                                   cv_ptr_negative_dx, cv_ptr_negative_dy, TS_id_, true));
    // keep TS_history's size constant
    while (TS_history_.size() > TS_HISTORY_LENGTH_)
    {
        auto it = TS_history_.begin();
        TS_history_.erase(it);
    }
}

void esvo2_Mapping::AACallback(const esvo2_core::ImagePtr &AA_left)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    // check timestamp consistency
    if (!TS_history_.empty())
    {
        static constexpr double max_time_diff_before_reset_s = 1;
        const timePoint stamp_last_image = TS_history_.rbegin()->first;
        const double dt = esvo2_core::timePointToSec(AA_left.header_stamp) - esvo2_core::timePointToSec(stamp_last_image);
        if (std::fabs(dt) >= max_time_diff_before_reset_s)
        {
            printf("Inconsistent frame timestamp detected <AACallback> (new: %f, old %f), resetting.",
                     esvo2_core::timePointToSec(AA_left.header_stamp), esvo2_core::timePointToSec(stamp_last_image));
            reset();
        }
    }

    esvo2_core::ImagePtr cv_ptr_left;//, cv_ptr_right; ptr_right is unused
    cv_ptr_left = AA_left; //cv_bridge::toCvCopy(AA_left, sensor_msgs::image_encodings::MONO8);

    // select the pixels with high event frequency
    int num_of_resultImg = 0, drift_t = 0;
    double persent_of_point = 1;
    EventQueue EQ_tmp;
    cv::Mat resultImg = cv_ptr_left.image->clone();

    std::vector<std::vector<std::pair<int, cv::Point>>> roi_events(x_patches_ * y_patches_);
    std::vector<int> num_of_roi(x_patches_ * y_patches_, 0);
    cv::Mat AA = cv::Mat::zeros(resultImg.size(), resultImg.type());
    std::vector<int> num_processed(x_patches_ * y_patches_, 0);
    for (int y = 0; y < resultImg.rows; y++)
    {
        for (int x = 0; x < resultImg.cols; x++)
        {
            if (resultImg.at<uchar>(y, x) > 0)
            {
                num_of_resultImg++;
                int index = (y / (int)ceil((double)resultImg.rows / (double)y_patches_)) * (x_patches_) +
                            (x / (int)ceil((double)resultImg.cols / (double)x_patches_));
                num_of_roi[index]++;
                roi_events[index].push_back(std::make_pair((int)resultImg.at<uchar>(y, x), cv::Point(x, y)));
            }
        }
    }
    std::vector<double> ratios(x_patches_ * y_patches_, 0);
    cv::Mat events_map = cv::Mat::zeros(cv_ptr_left.image->size(), cv_ptr_left.image->type());
    cv::cvtColor(events_map, events_map, cv::COLOR_GRAY2BGR);
    // Source - https://stackoverflow.com/a/6926473
    auto rd = std::random_device {}; 
    auto rng = std::default_random_engine { rd() };
    for (int i = 0; i < (x_patches_ * y_patches_); i++)
    {
        // shuffle and sort the event points to ensure sampling uniformity as much as possible
        std::shuffle(roi_events[i].begin(), roi_events[i].end(), rng);
        sort(roi_events[i].begin(), roi_events[i].end(),
             [](std::pair<double, cv::Point> a, std::pair<double, cv::Point> b) { return (a.first > b.first); });
        ratios[i] = (double)num_of_roi[i] / (double)num_of_resultImg * 0.75;
        for (int j = 0; j < std::min((std::size_t)(PROCESS_EVENT_NUM_AA_ * ratios[i]), roi_events[i].size() / 2); j++)
        {
            Event e;
            e.x = roi_events[i][j].second.x;
            e.y = roi_events[i][j].second.y;
            e.timestamp = esvo2_core::secondsToTimePoint(esvo2_core::timePointToSec(AA_left.header_stamp) + 0.0000001);
            EQ_tmp.push_back(e);
            drift_t++;
            AA.at<uchar>(e.y, e.x) = 255;
            num_processed[i] = j;
        }
    }

    int empty_num = PROCESS_EVENT_NUM_AA_ - EQ_tmp.size();
    if (empty_num > 0)
    {
        for (int i = 0; i < (x_patches_ * y_patches_); i++)
        {
            persent_of_point = std::min(((double)empty_num) / x_patches_ * y_patches_, 1.);
            for (int j = num_processed[i];
                 j < std::min((std::size_t)(PROCESS_EVENT_NUM_AA_ * ratios[i]) + empty_num / (x_patches_ * y_patches_),
                              roi_events[i].size() / 2);
                 j++)
            {
                Event e;
                e.x = roi_events[i][j].second.x;
                e.y = roi_events[i][j].second.y;
                e.timestamp = esvo2_core::secondsToTimePoint(esvo2_core::timePointToSec(AA_left.header_stamp) + 0.0000001);
                EQ_tmp.push_back(e);
                drift_t++;
                AA.at<uchar>(e.y, e.x) = 255;
                num_processed[i] = j;
            }
        }
    }
    for (const Event &e : EQ_tmp)
        events_left_.push_back(e);
    clearEventQueue(events_left_);
}

void esvo2_Mapping::refImuCallback(const std::shared_ptr<esvo2_core::ImuMsg> &msg)
{
    double t = esvo2_core::timePointToSec(msg->timestamp);
    double dx = msg->linear_acceleration[0];
    double dy = msg->linear_acceleration[1];
    double dz = msg->linear_acceleration[2];
    double rx = msg->angular_velocity[0];
    double ry = msg->angular_velocity[1];
    double rz = msg->angular_velocity[2];
    Eigen::Vector3d acc(dx, dy, dz);
    Eigen::Vector3d gyr(rx, ry, rz);
    mBuf.lock();
    accBuf.push(make_pair(t, acc));
    gyrBuf.push(make_pair(t, gyr));
    mBuf.unlock();
    return;
}

void esvo2_Mapping::reset()
{
    // mutual-thread communication with MappingThread.
    std::cout << "Coming into reset()";
    reset_promise_.set_value();
    std::cout << "(reset) The mapping thread future is waiting for the value.";
    mapping_thread_future_.get();
    std::cout << "(reset) The mapping thread future receives the value.";

    // clear all maintained data
    events_left_.clear();
    events_right_.clear();
    TS_history_.clear();
    tf_->clear();
    pc_color_->clear();
    pc_filtered_->clear();
    pc_near_->clear();
    pc_global_->clear();
    TS_id_ = 0;
    depthFramePtr_->clear();
    dqvDepthPoints_.clear();

    ebm_.resetParameters(BM_patch_size_X_, BM_patch_size_Y_, BM_min_disparity_, BM_max_disparity_, BM_step_,
                         BM_ZNCC_Threshold_, BM_bUpDownConfiguration_, BM_patch_size_X_2_, BM_patch_size_Y_2_);

    for (int i = 0; i < 2; i++)
        std::cout << "****************************************************";
    std::cout << "****************** RESET THE SYSTEM *********************";
    for (int i = 0; i < 2; i++)
        std::cout << "****************************************************\n\n";

    // restart the mapping thread
    reset_promise_ = std::promise<void>();
    mapping_thread_promise_ = std::promise<void>();
    reset_future_ = reset_promise_.get_future();
    mapping_thread_future_ = mapping_thread_promise_.get_future();
    setSystemStatus(SystemStatus::INITIALIZATION);
    std::thread MappingThread(&esvo2_Mapping::MappingLoop, this, std::move(mapping_thread_promise_),
                              std::move(reset_future_));
    MappingThread.detach();
}

// void esvo2_Mapping::onlineParameterChangeCallback(DVS_MappingStereoConfig &config, uint32_t level)
// {
// }

void esvo2_Mapping::publishMappingResults(DepthMap::Ptr depthMapPtr, Transformation tr, timePoint t)
{
    cv::Mat invDepthImage, stdVarImage, ageImage, costImage, eventImage, confidenceMap, invDepthImage_rel;

    invDepthImage = TS_obs_ptr_->second.img_left_.clone();
    visualizor_.plot_map(depthMapPtr, tools::InvDepthMap, invDepthImage, invDepth_max_range_, invDepth_min_range_,
                         stdVar_vis_threshold_, age_vis_threshold_);

    // VIZ PUBLISH -> not publishing anything right now
    // Skip publishing the inv depth map for now
    // publishImage(invDepthImage, t, invDepthMap_pub_);

    if (getSystemStatus() == SystemStatus::INITIALIZATION)
        publishPointCloud(depthMapPtr, tr, t);
    if (getSystemStatus() == SystemStatus::WORKING)
    {
        if (FusionStrategy_ == "CONST_FRAMES")
        {
            if (dqvDepthPoints_.size() == maxNumFusionFrames_)
                publishPointCloud(depthMapPtr, tr, t);
        }
        if (FusionStrategy_ == "CONST_POINTS")
        {
            std::size_t numFusionPoints = 0;
            for (std::size_t n = 0; n < dqvDepthPoints_.size(); n++)
                numFusionPoints += dqvDepthPoints_[n].size();
            if (numFusionPoints > 0.5 * maxNumFusionPoints_)
                publishPointCloud(depthMapPtr, tr, t);
        }
    }
}

void esvo2_Mapping::publishPointCloud(DepthMap::Ptr &depthMapPtr, Transformation &tr, timePoint &t)
{
    // sensor_msgs::PointCloud2::Ptr pc_to_publish(new sensor_msgs::PointCloud2);
    Eigen::Matrix<double, 4, 4> T_world_result = tr.getTransformationMatrix();

    pc_color_->clear();
    pc_color_->reserve(depthMapPtr->size());
    pc_filtered_->clear();
    pc_filtered_->reserve(depthMapPtr->size());
    pc_near_->clear();
    pc_near_->reserve(depthMapPtr->size());

    double FarthestDistance = 0.0;
    Eigen::Vector3d FarthestPoint;

    for (auto it = depthMapPtr->begin(); it != depthMapPtr->end(); it++)
    {
        Eigen::Vector3d p_world = T_world_result.block<3, 3>(0, 0) * it->p_cam() + T_world_result.block<3, 1>(0, 3);

        // set color for each point
        int index =
            floor((1 / it->p_cam().z() - invDepth_min_range_) / (invDepth_max_range_ - invDepth_min_range_) * 255.0f);
        if (index > 255)
            index = 255;
        if (index < 0)
            index = 0;
        pcl::PointXYZRGBL point;
        point.x = p_world(0);
        point.y = p_world(1);
        point.z = p_world(2);
        point.r = 255.0f * Visualization::r[index];
        point.g = 255.0f * Visualization::g[index];
        point.b = 255.0f * Visualization::b[index];

        // set label for each point: 1 for shown, 0 for hidden
        if (it->valid() && it->variance() < pow(stdVar_vis_threshold_, 2) && it->age() >= (int)age_vis_threshold_)
        {
            point.label = 1;
            pc_filtered_->push_back(point);
            if (it->p_cam().norm() < visualize_range_)
                pc_near_->push_back(pcl::PointXYZ(p_world(0), p_world(1), p_world(2)));
        }
        else
            point.label = 0;
        pc_color_->push_back(point);
    }

    // publish the local 3D map which is used by the tracker.
    if (!pc_color_->empty())
    {
        // --Publishing the point cloud to tracking 
        // pcl::toROSMsg(*pc_color_, *pc_to_publish);
        // pc_to_publish->header.stamp = t;
        // pc_pub_.publish(pc_to_publish);
        std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBL>> pointcloud_local_2 = make_shared<pcl::PointCloud<pcl::PointXYZRGBL>>();
        *pointcloud_local_2 = *pc_color_;
        pointcloud_Map_to_Track_.add(pointcloud_local_2, t);
    }
    if (!pc_filtered_->empty())
    {
        // pcl::toROSMsg(*pc_filtered_, *pc_to_publish);
        // pc_to_publish->header.stamp = t;
        // pc_filtered_pub_.publish(pc_to_publish);

        // VIZ PUBLISH -> not publishing anything right now
        std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBL>> pointcloud_filtered2 = make_shared<pcl::PointCloud<pcl::PointXYZRGBL>>();
        *pointcloud_filtered2 = *pc_filtered_;
        // timestamp is t if want to add to a queue
    }

    // publish global pointcloud
    if (bVisualizeGlobalPC_)
    {
        if (esvo2_core::timePointToSec(t) - t_last_pub_pc_ > visualizeGPC_interval_)
        {
            PointCloud::Ptr pc_filtered(new PointCloud());
            pcl::VoxelGrid<pcl::PointXYZ> sor;
            sor.setInputCloud(pc_near_);
            if (blarge_scale_)
                sor.setLeafSize(0.3, 0.3, 0.3); // Used in small scale environment.
            else
                sor.setLeafSize(0.01, 0.01, 0.01); // Used in large scale environment.
            sor.filter(*pc_filtered);

            // copy the most current pc tp pc_global
            std::size_t pc_length = pc_filtered->size();
            std::size_t numAddedPC = min(pc_length, numAddedPC_threshold_) - 1;
            pc_global_->insert(pc_global_->end(), pc_filtered->end() - numAddedPC, pc_filtered->end());
            // pcl::PCDWriter writer; // This was unused

            // publish point cloud
            // pcl::toROSMsg(*pc_global_, *pc_to_publish);
            // pc_to_publish->header.stamp = t;
            // gpc_pub_.publish(pc_to_publish);

            // VIZ PUBLISH -> not publishing anything right now
            // Should just pass reference to pc_global_ to visualizer in the constructor and it will auto update gg
            // timestamp is t if want to add to a queue

            t_last_pub_pc_ = esvo2_core::timePointToSec(t);
        }
    }
}

void esvo2_Mapping::publishImage(const cv::Mat &image, const timePoint &t, std::string encoding)
{
    // if (pub.getNumSubscribers() == 0)
        // return;

    // std_msgs::Header header;
    // header.stamp = t;
    // VIZ PUBLISH -> not being used right now, so are commenting it out. Was used to publish the inv depth map

    // sensor_msgs::ImagePtr msg = cv_bridge::CvImage(header, encoding.c_str(), image).toImageMsg();
    // pub.publish(msg);
}

void esvo2_Mapping::createEdgeMask(std::vector<Event *> &vEventsPtr, PerspectiveCamera::Ptr &camPtr, cv::Mat &edgeMap,
                                   std::vector<std::pair<std::size_t, std::size_t>> &vEdgeletCoordinates, bool bUndistortEvents,
                                   std::size_t radius)
{
    std::size_t col = camPtr->width_;
    std::size_t row = camPtr->height_;
    int dilate_radius = (int)radius;
    edgeMap = cv::Mat(cv::Size(col, row), CV_8UC1, cv::Scalar(0));
    vEdgeletCoordinates.reserve(col * row);

    auto it_tmp = vEventsPtr.begin();
    while (it_tmp != vEventsPtr.end())
    {
        // undistortion + rectification
        Eigen::Matrix<double, 2, 1> coor;
        if (bUndistortEvents)
            coor = camPtr->getRectifiedUndistortedCoordinate((*it_tmp)->x, (*it_tmp)->y);
        else
            coor = Eigen::Matrix<double, 2, 1>((*it_tmp)->x, (*it_tmp)->y);

        // assign
        int xcoor = std::floor(coor(0));
        int ycoor = std::floor(coor(1));

        for (int dy = -dilate_radius; dy <= dilate_radius; dy++)
            for (int dx = -dilate_radius; dx <= dilate_radius; dx++)
            {
                int x = xcoor + dx;
                int y = ycoor + dy;

                if (x < 0 || x >= col || y < 0 || y >= row)
                {
                }
                else
                {
                    edgeMap.at<uchar>(y, x) = 255;
                    vEdgeletCoordinates.emplace_back((std::size_t)x, (std::size_t)y);
                }
            }
        it_tmp++;
    }
}

void esvo2_Mapping::createDenoisingMask(std::vector<Event *> &vAllEventsPtr, cv::Mat &mask, std::size_t row, std::size_t col)
{
    cv::Mat eventMap;
    visualizor_.plot_eventMap(vAllEventsPtr, eventMap, row, col);
    cv::medianBlur(eventMap, mask, 3);
}

void esvo2_Mapping::extractDenoisedEvents(std::vector<Event *> &vCloseEventsPtr, std::vector<Event *> &vEdgeEventsPtr,
                                          cv::Mat &mask, std::size_t maxNum)
{
    vEdgeEventsPtr.reserve(vCloseEventsPtr.size());
    for (std::size_t i = 0; i < vCloseEventsPtr.size(); i++)
    {
        if (vEdgeEventsPtr.size() >= maxNum)
            break;
        if (vCloseEventsPtr[i]->x > mask.cols || vCloseEventsPtr[i]->y > mask.rows || vCloseEventsPtr[i]->x < 0 ||
            vCloseEventsPtr[i]->y < 0)
            continue;
        std::size_t x = vCloseEventsPtr[i]->x;
        std::size_t y = vCloseEventsPtr[i]->y;
        if (mask.at<uchar>(y, x) == 255)
            vEdgeEventsPtr.push_back(vCloseEventsPtr[i]);
    }
}

void esvo2_Mapping::getReprojection(std::vector<EventMatchPair> &vEMP, Eigen::Matrix4d T_last_now,
                                    std::vector<Event *> &vDenoisedEventsPtr_left_dy)
{
    for (auto event : vDenoisedEventsPtr_left_dy)
    {
        EventMatchPair emp;
        emp.x_left_ << camSysPtr_->cam_left_ptr_->getRectifiedUndistortedCoordinate(event->x, event->y);
        emp.invDepth_ = 0.1;
        emp.lr_depth = 1 / emp.invDepth_;
        emp.x_left_raw_ << event->x, event->y;
        vEMP.push_back(emp);
    }
    Eigen::Matrix3d R_last_now = T_last_now.block(0, 0, 3, 3);
    Eigen::Vector3d t_last_now = T_last_now.block(0, 3, 3, 1);
    Eigen::Matrix3d K = camSysPtr_->cam_left_ptr_->P_.block(0, 0, 3, 3);
    Eigen::Matrix3d K_inv = K.inverse();
    for (int i = 0; i < vEMP.size(); i++)
    {
        Eigen::Vector3d x_last, x_now;
        x_now << vEMP[i].x_left_(0), vEMP[i].x_left_(1), 1;
        double depth = 1 / vEMP[i].invDepth_;
        x_last = (R_last_now * (depth * K_inv * x_now) + t_last_now);
        x_last = K * x_last / x_last(2);
        vEMP[i].x_last_ << x_last(0), x_last(1);
    }
}

void esvo2_Mapping::selectPoint()
{

    // get gradient map
    cv::Mat events_map = cv::Mat::zeros(TS_obs_ptr_->second.img_left_.size(), CV_8U);

    vDenoisedEventsPtr_left_dx_.clear();
    vDenoisedEventsPtr_left_dy_.clear();
    vDenoisedEventsPtr_left_dx_.reserve(PROCESS_EVENT_NUM_);
    vDenoisedEventsPtr_left_dy_.reserve(PROCESS_EVENT_NUM_);

    // divide events by dx_dy
    for (auto event : vDenoisedEventsPtr_left_)
    {
        int redundant = 4;
        Eigen::Vector2d x;
        x << event->x, event->y;
        if (x(1) - redundant < redundant || x(1) + redundant >= TS_obs_ptr_->second.img_left_.rows ||
            x(0) - redundant < redundant || x(0) + redundant >= TS_obs_ptr_->second.img_left_.cols)
        {
            continue;
        }
        x = camSysPtr_->cam_left_ptr_->getRectifiedUndistortedCoordinate(event->x, event->y);
        if (x(1) - redundant < redundant || x(1) + redundant >= TS_obs_ptr_->second.img_left_.rows ||
            x(0) - redundant < redundant || x(0) + redundant >= TS_obs_ptr_->second.img_left_.cols)
        {
            continue;
        }
        double dx = abs(TS_obs_ptr_->second.dTS_negative_du_left_((int)x(1), (int)x(0)));
        double dy = abs(TS_obs_ptr_->second.dTS_negative_dv_left_((int)x(1), (int)x(0)));
        dx = ((dx <= 0.01) ? 0.1 : dx);
        dy = ((dy <= 0.01) ? 0.1 : dy);
        double dx_dy = (double)dx / (double)dy;
        if (dx_dy < eta_for_select_points_)
        {
            vDenoisedEventsPtr_left_dy_.push_back(event);
        }
        events_map.at<uchar>(x(1), x(0)) = 255;
        if (dx_dy >= eta_for_select_points_)
        {
            vDenoisedEventsPtr_left_dx_.push_back(event);
        }
    }
}

bool esvo2_Mapping::getIMUInterval(double t0, double t1, vector<pair<double, Eigen::Vector3d>> &accVector,
                                   vector<pair<double, Eigen::Vector3d>> &gyrVector)
{
    if (accBuf.empty())
    {
        std::cerr << "not receive imu data"<<std::endl;
        return false;
    }
    if (t1 <= accBuf.back().first)
    {
        while (accBuf.front().first <= t0)
        {
            accBuf.pop();
            gyrBuf.pop();
        }
        while (accBuf.front().first < t1)
        {
            accVector.push_back(accBuf.front());
            accBuf.pop();
            gyrVector.push_back(gyrBuf.front());
            gyrBuf.pop();
        }
        accVector.push_back(accBuf.front());
        gyrVector.push_back(gyrBuf.front());
    }
    else
        return false;
    return true;
}

void esvo2_Mapping::initFirstIMUPose(vector<pair<double, Eigen::Vector3d>> &accVector)
{
    std::cout << "init first imu pose";
    initFirstPoseFlag = true;
    // return;
    Eigen::Vector3d averAcc(0, 0, 0);
    int n = (int)accVector.size();
    for (std::size_t i = 0; i < accVector.size(); i++)
    {
        averAcc = averAcc + accVector[i].second;
    }
    averAcc = averAcc / n;
    Eigen::Matrix3d R0 = Utility::g2R(averAcc);
    double yaw = Utility::R2ypr(R0).x();
    R0 = Utility::ypr2R(Eigen::Vector3d{-yaw, 0, 0}) * R0;
    BackendOpt_.Rs[0] = R0;
}

void esvo2_Mapping::processIMU(double t, double dt, const Eigen::Vector3d &linear_acceleration,
                               const Eigen::Vector3d &angular_velocity)
{
    if (!first_imu)
    {
        first_imu = true;
        BackendOpt_.acc_0 = linear_acceleration;
        acc_0 = linear_acceleration;
        BackendOpt_.gyr_0 = angular_velocity;
        gyr_0 = angular_velocity;
    }

    if (!BackendOpt_.pre_integrations[BackendOpt_.frame_count])
    {
        BackendOpt_.pre_integrations[BackendOpt_.frame_count] =
            new IntegrationBase{BackendOpt_.acc_0, BackendOpt_.gyr_0, BackendOpt_.Bas[BackendOpt_.frame_count],
                                BackendOpt_.Bgs[BackendOpt_.frame_count], BackendOpt_.g_optimal};
    }
    if (BackendOpt_.frame_count != 0)
    {
        BackendOpt_.pre_integrations[BackendOpt_.frame_count]->push_back(dt, linear_acceleration, angular_velocity);
    }
    BackendOpt_.acc_0 = linear_acceleration;
    BackendOpt_.gyr_0 = angular_velocity;
    acc_0 = linear_acceleration;
    gyr_0 = angular_velocity;
}
} // namespace esvo2_core