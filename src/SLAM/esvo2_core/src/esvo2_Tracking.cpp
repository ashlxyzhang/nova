#include <esvo2_core/esvo2_Tracking.h>
#include <esvo2_core/tools/TicToc.h>
#include <minkindr_conversions/kindr_tf.h>
#include <sys/stat.h>
#include <tf/transform_broadcaster.h>
#include <data_passing.h>
#include <multi_data_passing.h>

// #define ESVO2_CORE_TRACKING_DEBUG
// #define ESVO2_CORE_TRACKING_DEBUG
namespace esvo2_core
{
esvo2_Tracking::esvo2_Tracking(const YAML::Node &config,
        DataPassingDeque<esvo2_core::PoseStamped>* stamped_pose_Track_to_Map,
        DataPassingDeque<esvo2_core::PoseStamped>* stamped_pose_Track_to_Track)
    : config_(config), calibInfoDir_(config_["calibInfoDir"].as<std::string>("")),
      camSysPtr_(new CameraSystem(calibInfoDir_, false)),
      rpConfigPtr_(new RegProblemConfig(
          config_["patch_size_X"].as<int>(25), config_["patch_size_Y"].as<int>(25), config_["kernelSize"].as<int>(15),
          config_["LSnorm"].as<std::string>("l2"), config_["huber_threshold"].as<double>(10.0),
          config_["invDepth_min_range"].as<double>(0.0), config_["invDepth_max_range"].as<double>(0.0),
          config_["MIN_NUM_EVENTS"].as<int>(1000), config_["MAX_REGISTRATION_POINTS"].as<int>(500),
          config_["BATCH_SIZE"].as<int>(200), config_["MAX_ITERATION"].as<int>(10))),
      rpType_((RegProblemType)((size_t)config_["RegProblemType"].as<int>(0))),
      rpSolver_(camSysPtr_, rpConfigPtr_, rpType_, NUM_THREAD_TRACKING),
      imu_data_(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(),
                g_optimal),
      ets_(IDLE)
{
    //queues
    this->stamped_pose_Track_to_Map = stamped_pose_Track_to_Map;
    this->stamped_pose_Track_to_Track = stamped_pose_Track_to_Track;

    // offline data
    dvs_frame_id_ = config_["dvs_frame_id"].as<std::string>("dvs");
    world_frame_id_ = config_["world_frame_id"].as<std::string>("world");

    /**** online parameters ***/
    tracking_rate_hz_ = config_["tracking_rate_hz"].as<int>(100);
    TS_HISTORY_LENGTH_ = config_["TS_HISTORY_LENGTH"].as<int>(100);
    REF_HISTORY_LENGTH_ = config_["REF_HISTORY_LENGTH"].as<int>(5);
    bSaveTrajectory_ = config_["SAVE_TRAJECTORY"].as<bool>(false);
    bVisualizeTrajectory_ = config_["VISUALIZE_TRAJECTORY"].as<bool>(true);
    bUseImu_ = config_["USE_IMU"].as<bool>(true);
    resultPath_ = config_["PATH_TO_SAVE_TRAJECTORY"].as<std::string>(std::string());
    setSystemStatus(SystemStatus::INITIALIZATION);

    // get extrinsic parameters
    R_b_c_ = camSysPtr_->cam_left_ptr_->T_b_c_.block<3, 3>(0, 0);

    imu_data_.initialization(ba_, bg_);
    initVsFlag = false;

    // TS_negaTS_sync_.registerCallback(boost::bind(&esvo2_Tracking::timeSurface_NegaTS_Callback, this, _1, _2, _3, _4));

    tf_ = std::make_shared<tf::Transformer>(true, ros::Duration(100.0));
    // pose_pub_ = nh_.advertise<geometry_msgs::PoseStamped>("/esvo2_tracking/pose_pub", 1);
    path_pub_ = nh_.advertise<nav_msgs::Path>("/esvo2_tracking/trajectory", 1);
    // map_sub_ = nh_.subscribe("pointcloud", 0, &esvo2_Tracking::refMapCallback, this); // local map in the ref view.
    // stampedPose_sub_ = nh_.subscribe("stamped_pose", 0, &esvo2_Tracking::stampedPoseCallback,
    //                                  this); // for accessing the pose of the ref view.
    imu_sub_ = nh_.subscribe("/imu/data", 0, &esvo2_Tracking::refImuCallback, this); // local map in the ref view.
    // V_ba_bg_sub_ = nh_.subscribe("/esvo2_mapping/V_ba_bg", 0, &esvo2_Tracking::VBaBgCallback, this);
    /*** For Visualization and Test ***/
    reprojMap_pub_left_ = it_.advertise("Reproj_Map_Left", 1);
    rpSolver_.setRegPublisher(&reprojMap_pub_left_);

    // rename the old trajectory file
    renameOldTraj();

    /*** Tracker ***/
    T_world_cur_ = Eigen::Matrix<double, 4, 4>::Identity();
    t_world_cur_ = last_t_world_cur_ = last_t_ = Eigen::Vector3d::Zero();
    std::thread TrackingThread(&esvo2_Tracking::TrackingLoop, this);
    TrackingThread.detach();
}

esvo2_Tracking::~esvo2_Tracking()
{
    if (!resultPath_.empty())
    {
        LOG(INFO) << "pose size: " << lPose_.size();
        string path = std::string(resultPath_ + "result.txt");
        saveTrajectory(path);
    }
    pose_pub_.shutdown();
}

void esvo2_Tracking::TrackingLoop()
{
    ros::Rate r(tracking_rate_hz_);
    while (ros::ok())
    {
        // Keep Idling
        if (refPCMap_.size() < 1 || TS_history_.size() < 1)
        {
            r.sleep();
            continue;
        }

        // Reset
        if (getSystemStatus() == SystemStatus::INITIALIZATION &&
            ets_ == WORKING) // This is true when the system is reset from dynamic reconfigure
        {
            reset();
            r.sleep();
            continue;
        }
        if (getSystemStatus() == SystemStatus::TERMINATE)
        {
            LOG(INFO) << "The tracking node is terminated manually...";
            break;
        }
        TicToc tt;
        double curData_time;
        // Data Transfer (If mapping node had published refPC.)
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (esvo2_core::timePointToSec(ref_.t_) < esvo2_core::timePointToSec(refPCMap_.rbegin()->first)) // new reference map arrived
                refDataTransferring();
            if (esvo2_core::timePointToSec(cur_.t_) < esvo2_core::timePointToSec(TS_history_.rbegin()->first)) // new observation arrived
            {
                if (esvo2_core::timePointToSec(ref_.t_) >= esvo2_core::timePointToSec(TS_history_.rbegin()->first))
                {
                    LOG(INFO) << "The time_surface observation should be obtained after the reference frame";
                    exit(-1);
                }
                if (!curDataTransferring())
                {
                    continue;
                }
            }
            else
            {
                continue;
            }
        }
        curData_time = tt.toc();

        // create new regProblem
        double t_resetRegProblem, t_solve, t_pub_result;

        if (rpSolver_.resetRegProblem(&ref_, &cur_))
        {
            if (ets_ == IDLE)
                ets_ = WORKING;
            if (getSystemStatus() != SystemStatus::WORKING)
            {
                setSystemStatus(SystemStatus::WORKING);
                LOG(INFO) << "ESVO2_SYSTEM_STATUS: WORKING";
            }

            // TicToc t_coarse;
            if (rpType_ == REG_NUMERICAL)
                rpSolver_.solve_numerical();
            if (rpType_ == REG_ANALYTICAL)
                rpSolver_.solve_analytical();

            T_world_cur_ = cur_.tr_.getTransformationMatrix();
            t_world_cur_ = T_world_cur_.block(0, 3, 3, 1);
            publishPose(cur_.t_, cur_.tr_);
            if (bVisualizeTrajectory_)
                publishPath(cur_.t_, cur_.tr_);

            // save result and gt if available.
            if (bSaveTrajectory_)
            {
                // save results to listPose and listPoseGt
                lTimestamp_.push_back(std::to_string(esvo2_core::timePointToSec(cur_.t_)));
                lPose_.push_back(cur_.tr_.getTransformationMatrix());
            }
        }
        else
        {
            setSystemStatus(SystemStatus::INITIALIZATION);
            ets_ = IDLE;
        }
        std::ofstream f;

#ifdef ESVO2_CORE_TRACKING_LOG
        double t_overall_count = 0;
        t_overall_count = t_resetRegProblem + t_solve + t_pub_result;
        LOG(INFO) << "\n";
        LOG(INFO) << "------------------------------------------------------------";
        LOG(INFO) << "--------------------Tracking Computation Cost---------------";
        LOG(INFO) << "------------------------------------------------------------";
        LOG(INFO) << "ResetRegProblem: " << t_resetRegProblem << " ms, (" << t_resetRegProblem / t_overall_count * 100
                  << "%).";
        LOG(INFO) << "Registration: " << t_solve << " ms, (" << t_solve / t_overall_count * 100 << "%).";
        LOG(INFO) << "pub result: " << t_pub_result << " ms, (" << t_pub_result / t_overall_count * 100 << "%).";
        LOG(INFO) << "Total Computation (" << rpSolver_.lmStatics_.nPoints_ << "): " << t_overall_count << " ms.";
        LOG(INFO) << "------------------------------------------------------------";
        LOG(INFO) << "------------------------------------------------------------";
#endif
        r.sleep();
    } // while
}

bool esvo2_Tracking::refDataTransferring()
{
    // load reference info.
    ref_.t_ = esvo2_core::secToTimePoint(esvo2_core::timePointToSec(refPCMap_.rbegin()->first));
    timePoint t = esvo2_core::secToTimePoint(esvo2_core::timePointToSec(refPCMap_.rbegin()->first) - 0.001);
    if (getSystemStatus() == SystemStatus::INITIALIZATION && ets_ == IDLE)
        ref_.tr_.setIdentity();
    if (getSystemStatus() == SystemStatus::WORKING ||
        (getSystemStatus() == SystemStatus::INITIALIZATION && ets_ == WORKING))
    {
        if (!getPoseAt(t, ref_.tr_, dvs_frame_id_))
        {
            LOG(INFO) << "system_status: " << systemStatusToString() << ", ref_.t_: " << ref_.t_.toNSec();
            LOG(INFO)
                << "Logic error ! There must be a pose for the given timestamp, because mapping has been finished.";
            // exit(-1);
            return false;
        }
    }

    // get the point cloud
    size_t numPoint = refPCMap_.rbegin()->second->size();
    ref_.vPointXYZPtr_.clear();
    ref_.vPointXYZPtr_.reserve(numPoint);
    auto PointXYZ_begin_it = refPCMap_.rbegin()->second->begin();
    auto PointXYZ_end_it = refPCMap_.rbegin()->second->end();
    while (PointXYZ_begin_it != PointXYZ_end_it)
    {
        ref_.vPointXYZPtr_.push_back(PointXYZ_begin_it.base()); // Copy the pointer of the pointXYZ
        PointXYZ_begin_it++;
    }
    return true;
}

bool esvo2_Tracking::curDataTransferring()
{
    // load current observation
    auto ev_last_it = EventBuffer_lower_bound(events_left_, cur_.t_);
    auto TS_it = TS_history_.rbegin();

    // TS_history may not be updated before the tracking loop excutes the data transfering
    if (cur_.t_ == TS_it->first)
        return false;
    cur_.t_ = TS_it->first;
    cur_.pTsObs_ = &TS_it->second;

    if (getSystemStatus() == SystemStatus::INITIALIZATION && ets_ == IDLE)
    {
        cur_.tr_ = ref_.tr_;
    }
    if (getSystemStatus() == SystemStatus::WORKING ||
        (getSystemStatus() == SystemStatus::INITIALIZATION && ets_ == WORKING))
    {
        curImuTransferring();
        // if use imu, the pose of the current frame is updated in the imu preintegration.
        if (bUseImu_)
        {
            Eigen::Matrix3d R_w_c = T_world_cur_.block(0, 0, 3, 3);

            Eigen::Quaterniond q1 = Eigen::Quaterniond::Identity();
            Eigen::Quaterniond q = q1.slerp(1, Imu_q);

            if (t_world_cur_ != Eigen::Vector3d::Zero() && last_t_world_cur_ != Eigen::Vector3d::Zero())
            {
                last_t_ = t_world_cur_ - last_t_world_cur_;
                last_t_world_cur_ = T_world_cur_.block(0, 3, 3, 1);
                qprevious_ts_.push_back(last_t_);
                int average_window_size = 5;
                if (qprevious_ts_.size() > average_window_size)
                {
                    qprevious_ts_.erase(qprevious_ts_.begin(),
                                        qprevious_ts_.begin() + qprevious_ts_.size() - average_window_size);
                }
                if (qprevious_ts_.size() > 3)
                {
                    last_t_.setZero();
                    for (Eigen::Vector3d t : qprevious_ts_)
                    {
                        last_t_ += t;
                    }
                    last_t_ = last_t_ / qprevious_ts_.size();
                }

                // If the predicted position change is significantly different from the previous displacement due to
                // potentially unstable velocity estimates, use the previous displacement as the initial value for the
                // next optimization.
                if (initVsFlag &&
                    (imu_data_.t_v_last_mapping.second * imu_data_.sum_dt - last_t_).norm() / last_t_.norm() < 0.1)
                    T_world_cur_.block(0, 3, 3, 1) +=
                        R_b_c_.transpose() * Imu_t + (imu_data_.t_v_last_mapping.second * imu_data_.sum_dt);
                else
                    T_world_cur_.block(0, 3, 3, 1) += R_b_c_.transpose() * Imu_t + last_t_;
            }
            else
            {
                last_t_world_cur_ = T_world_cur_.block(0, 3, 3, 1);
            }

            T_world_cur_.block(0, 0, 3, 3) = R_w_c * R_b_c_ * q.toRotationMatrix() * R_b_c_.inverse();
        }
        Eigen::Matrix3d R_w_c = T_world_cur_.block(0, 0, 3, 3);
        T_world_cur_.block(0, 0, 3, 3) = fixRotationMatrix(R_w_c);
        cur_.tr_ = Transformation(T_world_cur_);
    }
    return true;
}

bool esvo2_Tracking::curImuTransferring()
{
    auto TS_it = TS_history_.rbegin();
    double cur_TS_time = esvo2_core::timePointToSec(TS_it->first);
    double last_TS_time = esvo2_core::timePointToSec((++TS_it)->first);

    if (bUseImu_)
    {
        imu_mutex_.lock();
        if (initVsFlag && (esvo2_core::timePointToSec(refPCMap_.rbegin()->first) - imu_data_.t_v_last_mapping.first > 0.001))
            imu_data_.update_v(esvo2_core::timePointToSec(refPCMap_.rbegin()->first), last_TS_time);
        imu_data_.getPose(last_TS_time, cur_TS_time, true, esvo2_core::timePointToSec(ref_.t_));
        if (t_world_cur_ != Eigen::Vector3d::Zero() && last_t_world_cur_ != Eigen::Vector3d::Zero())
        {
            Eigen::Matrix3d R_w_c = T_world_cur_.block(0, 0, 3, 3);
            imu_data_.t_v_last_mapping.second += R_b_c_ * R_w_c.transpose() * imu_data_.delta_v;
        }
        imu_mutex_.unlock();
    }

    Eigen::Vector3d delta_p = imu_data_.delta_p;
    Eigen::Quaterniond delta_q = imu_data_.delta_q;

    Imu_q = imu_data_.delta_q;
    Imu_t = imu_data_.delta_p;
    return true;
}

void esvo2_Tracking::reset()
{
    // clear all maintained data
    ets_ = IDLE;
    TS_id_ = 0;
    TS_history_.clear();
    refPCMap_.clear();
    events_left_.clear();
}

/********************** Callback functions *****************************/
void esvo2_Tracking::refImuCallback(const std::shared_ptr<esvo2_core::ImuMsg> &msg)
{
    std::lock_guard<std::mutex> lock(imu_mutex_);
    Eigen::Vector3d acc, gyr;
    if (imu_data_.dt_buf.size() == 0)
    {

        acc[0] = msg->linear_acceleration[0];
        acc[1] = msg->linear_acceleration[1];
        acc[2] = msg->linear_acceleration[2];

        gyr[0] = msg->angular_velocity[0];
        gyr[1] = msg->angular_velocity[1];
        gyr[2] = msg->angular_velocity[2];
        if (imu_data_.last_time == 0)
        {
            imu_data_.begin_time = esvo2_core::timePointToSec(msg->timestamp);
            imu_data_.push_back(0.001, acc, gyr);
            imu_data_.last_time = imu_data_.begin_time;
        }
        else
        {
            double dt = esvo2_core::timePointToSec(msg->timestamp) - imu_data_.last_time;
            if (dt < 0)
                return;
            imu_data_.begin_time = esvo2_core::timePointToSec(msg->timestamp);
            imu_data_.push_back(dt, acc, gyr);
        }
    }
    else
    {
        double time = esvo2_core::timePointToSec(msg->timestamp);
        double dt = time - imu_data_.last_time;
        if (dt < 0)
            return;
        acc[0] = msg->linear_acceleration[0];
        acc[1] = msg->linear_acceleration[1];
        acc[2] = msg->linear_acceleration[2];

        gyr[0] = msg->angular_velocity[0];
        gyr[1] = msg->angular_velocity[1];
        gyr[2] = msg->angular_velocity[2];
        imu_data_.push_back(dt, acc, gyr);
        imu_data_.last_time = time;
    }
}

void esvo2_Tracking::VBaBgCallback(const std::shared_ptr<esvo2_core::VBaBg> &msg)
{
    Eigen::Vector3d g_temp, ba_temp, bg_temp, V_temp;
    double t_temp = esvo2_core::timePointToSec(msg->head);
    for (int i = 0; i < 3; i++)
    {
        g_temp(i) = msg.g[i];
        ba_temp(i) = msg.ba[i];
        bg_temp(i) = msg.bg[i];
        V_temp(i) = msg.Vs[i];
    }
    imu_mutex_.lock();
    imu_data_.G = g_temp;
    imu_data_.linearized_ba = ba_temp;
    imu_data_.linearized_bg = bg_temp;
    imu_data_.t_v_last_mapping = std::make_pair(t_temp, V_temp);
    imu_mutex_.unlock();
    initVsFlag = true;
}

void esvo2_Tracking::refMapCallback(const std::pair<std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBL>>, timePoint> &msg)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    // pcl::PCLPointCloud2 pcl_pc;
    // pcl_conversions::toPCL(*msg, pcl_pc);
    // pcl::PointCloud<pcl::PointXYZRGBL>::Ptr PC_ptr(new pcl::PointCloud<pcl::PointXYZRGBL>);
    // pcl::fromPCLPointCloud2(pcl_pc, *PC_ptr);
    pcl::PointCloud<pcl::PointXYZRGBL>::Ptr PC_ptr = msg.first->make_shared();

    // refPCMap_.emplace(msg->header.stamp, PC_ptr);
    refPCMap_.emplace(msg.second, PC_ptr);
    while (refPCMap_.size() > REF_HISTORY_LENGTH_)
    {
        auto it = refPCMap_.begin();
        refPCMap_.erase(it);
    }
}

void esvo2_Tracking::eventsCallback(const EventArray::ConstPtr &msg)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    // add new ones and remove old ones
    for (const Event &e : msg->events)
    {
        events_left_.push_back(e);
        int i = events_left_.size() - 2;
        while (i >= 0 && events_left_[i].ts > e.ts) // we may have to sort the queue, just in case the raw event
                                                    // messages do not come in a chronological order.
        {
            events_left_[i + 1] = events_left_[i];
            i--;
        }
        events_left_[i + 1] = e;
    }
    clearEventQueue();
}

void esvo2_Tracking::clearEventQueue()
{
    static constexpr size_t MAX_EVENT_QUEUE_LENGTH = 5000000;
    if (events_left_.size() > MAX_EVENT_QUEUE_LENGTH)
    {
        size_t remove_events = events_left_.size() - MAX_EVENT_QUEUE_LENGTH;
        events_left_.erase(events_left_.begin(), events_left_.begin() + remove_events);
    }
}

void esvo2_Tracking::timeSurface_NegaTS_Callback(const esvo2_core::ImagePtr &time_surface_left,
                                                 const esvo2_core::ImagePtr &time_surface_negative,
                                                 const esvo2_core::ImagePtr &time_surface_dx,
                                                 const esvo2_core::ImagePtr &time_surface_dy)
{

    cv::Mat cv_ptr_left, cv_ptr_negative, cv_ptr_dx, cv_ptr_dy;
    try
    {
        cv_ptr_left = *(time_surface_left.image); //cv_bridge::toCvCopy(time_surface_left, sensor_msgs::image_encodings::MONO8);
        cv_ptr_negative = *(time_surface_negative.image); //cv_bridge::toCvCopy(time_surface_negative, sensor_msgs::image_encodings::MONO8);
        cv_ptr_dx = *(time_surface_dx.image); //cv_bridge::toCvCopy(time_surface_dx, sensor_msgs::image_encodings::TYPE_16SC1);
        cv_ptr_dy = *(time_surface_dy.image); //cv_bridge::toCvCopy(time_surface_dy, sensor_msgs::image_encodings::TYPE_16SC1);
    }
    catch (cv_bridge::Exception &e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }
    std::lock_guard<std::mutex> lock(data_mutex_);
    // push back the most current TS.
    timePoint t_new_ts = time_surface_left->header_stamp;
    TS_history_.emplace(t_new_ts,
                        TimeSurfaceObservation(cv_ptr_left, cv_ptr_negative, cv_ptr_dx, cv_ptr_dy, TS_id_, false));
    TS_id_++;

    // keep TS_history_'s size constant
    while (TS_history_.size() > TS_HISTORY_LENGTH_)
    {
        auto it = TS_history_.begin();
        TS_history_.erase(it);
    }
}

void esvo2_Tracking::stampedPoseCallback(const std::shared_ptr<esvo2_core::PoseStamped> &msg)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    // add pose to tf
    tf::Transform tf(tf::Quaternion(msg->orientation[0], msg->orientation[1], msg->pose.orientation[2],
                                    msg->pose.orientation[3]),
                     tf::Vector3(msg->position[0], msg->position[1], msg->position[2]));
    tf::StampedTransform st(tf, msg.head, msg->frame_id, dvs_frame_id_.c_str());
    tf_->setTransform(st);
    // broadcast the tf such that the nav_path messages can find the valid fixed frame "map".
    static tf::TransformBroadcaster br;
    br.sendTransform(st);
}

bool esvo2_Tracking::getPoseAt(const timePoint &t, esvo2_core::Transformation &Tr, const std::string &source_frame)
{
    std::string *err_msg = new std::string();
    if (!tf_->canTransform(world_frame_id_, source_frame, t, err_msg))
    {
        LOG(WARNING) << t.toNSec() << " : " << *err_msg;
        delete err_msg;
        return false;
    }
    else
    {
        tf::StampedTransform st;
        tf_->lookupTransform(world_frame_id_, source_frame, t, st);
        tf::transformTFToKindr(st, &Tr);
        return true;
    }
}

/************ publish results *******************/
void esvo2_Tracking::publishPose(const timePoint &t, Transformation &tr)
{
    // geometry_msgs::PoseStampedPtr ps_ptr(new geometry_msgs::PoseStamped());
    std::shared_ptr<esvo2_core::PoseStamped> ps_ptr = make_shared<esvo2_core::PoseStamped>();
    // ps_ptr->header.stamp = t;
    ps_ptr->timestamp = t;
    // ps_ptr->header.frame_id = world_frame_id_;
    ps_ptr->frame_id = world_frame_id_;
    // ps_ptr->pose.position.x = tr.getPosition()(0);
    // ps_ptr->pose.position.y = tr.getPosition()(1);
    // ps_ptr->pose.position.z = tr.getPosition()(2);
    ps_ptr->position[0] = tr.getPosition()(0);
    ps_ptr->position[1] = tr.getPosition()(1);
    ps_ptr->position[2] = tr.getPosition()(2);
    // ps_ptr->pose.orientation.x = tr.getRotation().x();
    // ps_ptr->pose.orientation.y = tr.getRotation().y();
    // ps_ptr->pose.orientation.z = tr.getRotation().z();
    // ps_ptr->pose.orientation.w = tr.getRotation().w();
    ps_ptr->rotation[0] = tr.getRotation().x();
    ps_ptr->rotation[1] = tr.getRotation().y();
    ps_ptr->rotation[2] = tr.getRotation().z();
    ps_ptr->rotation[3] = tr.getRotation().w();
    // pose_pub_.publish(ps_ptr);
    // Can send same ptr to both because the callback functions treat the pose as const
    stamped_pose_Track_to_Map.add(ps_ptr, t);
    stamped_pose_Track_to_Track.add(ps_ptr, t);

    if (!resultPath_.empty())
    {
        std::ofstream f;
        f.open(resultPath_ + "stamped_traj_estimate_ours.txt", std::ofstream::app);
        f << std::fixed;
        Eigen::Matrix3d Rwc_result;
        Eigen::Vector3d twc_result;
        f.setf(std::ios::fixed, std::ios::floatfield);
        f.precision(9);
        f << t << " ";
        f.precision(5);
        f << ps_ptr->pose.position.x << " " << ps_ptr->pose.position.y << " " << ps_ptr->pose.position.z << " "
          << ps_ptr->pose.orientation.x << " " << ps_ptr->pose.orientation.y << " " << ps_ptr->pose.orientation.z << " "
          << ps_ptr->pose.orientation.w << endl;
        f.close();
    }
}

void esvo2_Tracking::publishPath(const timePoint &t, Transformation &tr)
{
    std::shared_ptr<esvo2_core::PoseStamped> ps_ptr = make_shared<esvo2_core::PoseStamped>();
    
    ps_ptr->timestamp = t;
    ps_ptr->frame_id = world_frame_id_;
    ps_ptr->position[0] = tr.getPosition()(0);
    ps_ptr->position[1] = tr.getPosition()(1);
    ps_ptr->position[2] = tr.getPosition()(2);
    ps_ptr->orientation[0] = tr.getRotation().x();
    ps_ptr->orientation[1] = tr.getRotation().y();
    ps_ptr->orientation[2] = tr.getRotation().z();
    ps_ptr->orientation[3] = tr.getRotation().w();
    path_.header.stamp = t;
    path_.header.frame_id = world_frame_id_;
    path_.poses.push_back(*ps_ptr);
    path_pub_.publish(path_);
}

void esvo2_Tracking::saveTrajectory(std::string &resultDir)
{
    LOG(INFO) << "Saving trajectory to " << resultPath_ + "stamped_traj_estimate.txt" << " ......";

    std::ofstream f;
    f.open(resultPath_ + "stamped_traj_estimate.txt", std::ofstream::app);
    if (!f.is_open())
    {
        LOG(INFO) << "File at " << resultPath_ + "stamped_traj_estimate.txt"
                  << " is not opened, save trajectory failed.";
        exit(-1);
    }
    f << std::fixed;

    std::list<Eigen::Matrix<double, 4, 4>, Eigen::aligned_allocator<Eigen::Matrix<double, 4, 4>>>::iterator
        result_it_begin = lPose_.begin();
    std::list<Eigen::Matrix<double, 4, 4>, Eigen::aligned_allocator<Eigen::Matrix<double, 4, 4>>>::iterator
        result_it_end = lPose_.end();
    std::list<std::string>::iterator ts_it_begin = lTimestamp_.begin();

    for (; result_it_begin != result_it_end; result_it_begin++, ts_it_begin++)
    {
        Eigen::Matrix3d Rwc_result;
        Eigen::Vector3d twc_result;
        Rwc_result = (*result_it_begin).block<3, 3>(0, 0);
        twc_result = (*result_it_begin).block<3, 1>(0, 3);
        Eigen::Quaterniond q(Rwc_result);
        f.setf(std::ios::fixed, std::ios::floatfield);
        f.precision(9);
        f << *ts_it_begin << " ";
        f.precision(5);
        f << twc_result.transpose().x() << " " << twc_result.transpose().y() << " " << twc_result.transpose().z() << " "
          << q.x() << " " << q.y() << " " << q.z() << " " << q.w() << endl;
    }
    f.close();
    LOG(INFO) << "Saving trajectory to " << resultPath_ + "stamped_traj_estimate.txt" << ". Done !!!!!!.";
}

void esvo2_Tracking::renameOldTraj()
{
    string ori_name = resultPath_ + "stamped_traj_estimate_ours.txt";
    string new_name = resultPath_ + "traj_ours_old.txt";
    if (std::rename(ori_name.c_str(), new_name.c_str()) == 0)
    {
        LOG(INFO) << "\33[32m" << "File renamed successfully." << "\33[0m";
    }
    else
    {
        LOG(INFO) << "\33[33m" << "Failed to rename the file." << "\33[0m";
    }
}

void esvo2_Tracking::groundTruthCallback(const geometry_msgs::PoseStampedConstPtr &msg)
{
    std::ofstream f;
    f.open("/home/njk/output/ESVO2/stamped_groundtruth.txt", std::ofstream::app);
    f << std::fixed;
    f.setf(std::ios::fixed, std::ios::floatfield);
    f.precision(9);
    f << esvo2_core::timePointToSec(msg->header.stamp) << " ";
    f.precision(5);
    f << msg->pose.position.x << " " << msg->pose.position.y << " " << msg->pose.position.z << " "
      << msg->pose.orientation.x << " " << msg->pose.orientation.y << " " << msg->pose.orientation.z << " "
      << msg->pose.orientation.w << endl;
    f.close();
}

Eigen::Matrix3d esvo2_Tracking::fixRotationMatrix(const Eigen::Matrix3d &R)
{
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(R, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Matrix3d U = svd.matrixU();
    Eigen::Matrix3d V = svd.matrixV();
    return U * V.transpose();
}

} // namespace esvo2_core
