// #include <glog/logging.h>
#include "image_representation/ImageRepresentation.h"
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/core/eigen.hpp>
#include <opencv2/opencv.hpp>
#include "data_passing.hh"
#include "multi_data_passing.hh"
#include "esvo2_core/tools/types.h"

#include <fstream>
#include <cmath>
#include <vector>

// #define ESVIO_REPRESENTATION_LOG

namespace image_representation
{
ImageRepresentation::ImageRepresentation(std::atomic<bool> &is_running_, const YAML::Node &config,  
            const std::string& camera_yaml_path,
            MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat>& multi_to_Track, 
            MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat>& multi_to_Map,
            DataPassingDeque<cv::Mat>& AA_left_IR_to_Map)
             : is_running(is_running_), config_(config), multi_to_Track_(multi_to_Track), multi_to_Map_(multi_to_Map), AA_left_IR_to_Map_(AA_left_IR_to_Map)
{
    // setup subscribers and publishers
    // event_sub_ = nh_.subscribe("events", 0, &ImageRepresentation::eventsCallback, this);
    // image_transport::ImageTransport it_(nh_);
    is_left_ = config_["is_left"].as<bool>(true);
    // if (is_left_)
    // {
    //     image_representation_pub_TS_ = it_.advertise("image_representation_TS_", 5); // for block matching
    //     image_representation_pub_negative_TS_ =
    //         it_.advertise("image_representation_negative_TS_", 5); // negative OS-TS for 3D-2D regristration
    //     image_representation_pub_AA_frequency_ = it_.advertise("image_representation_AA_frequency_", 5);
    //     image_representation_pub_AA_mat_ =
    //         it_.advertise("image_representation_AA_mat_", 5); // for temporal stereo matching
    //     dx_image_pub_ = it_.advertise("dx_image_pub_", 5);    // gradient map for point sampling
    //     dy_image_pub_ = it_.advertise("dy_image_pub_", 5);
    // }
    // else
    // {
    //     image_representation_pub_TS_ = it_.advertise("image_representation_TS_", 5);
    // }
    bUse_Sim_Time_ = config_["use_sim_time"].as<bool>(true);

    // system variables
    int representation_mode;
    representation_mode = config_["representation_mode"].as<int>(0);
    median_blur_kernel_size_ = config_["median_blur_kernel_size"].as<int>(1);
    blur_size_ = config_["blur_size"].as<int>(7);
    max_event_queue_length_ = config_["max_event_queue_len"].as<int>(20);

    representation_mode_ = (RepresentationMode)representation_mode;

    // rectify variables
    bCamInfoAvailable_ = false;
    bSensorInitialized_ = false;
    sensor_size_ = cv::Size(0, 0);

    // local parameters
    bUseStereoCam_ = config_["use_stereo_cam"].as<bool>(true);
    decay_ms_ = config_["decay_ms"].as<double>(30);
    decay_sec_ = decay_ms_ / 1000.0;
    x_patches_ = config_["x_patches"].as<int>(8);
    y_patches_ = config_["y_patches"].as<int>(6);
    generation_rate_hz_ = config_["generation_rate_hz"].as<int>(100);
    // calibInfoDir_ = config_["calibInfoDir"].as<std::string>("path is not given");
    if (!loadCalibInfo(camera_yaml_path, is_left_))
    {
        printf("Load Calib Info Error!!!  Given path is: %s \n", calibInfoDir_.c_str());
    }

    if (is_left_)
        std::cout << "\33[32m" << "Left event representation node is up " << "\33[0m"<<std::endl;
    else
        std::cout << "\33[32m" << "Right event representation node is up " << "\33[0m"<<std::endl;

    // start generation
    std::thread GenerationThread(&ImageRepresentation::GenerationLoop, this);
    GenerationThread.detach();
}

ImageRepresentation::~ImageRepresentation()
{
    // dx_image_pub_.shutdown();
    // dy_image_pub_.shutdown();
    // image_representation_pub_TS_.shutdown();
    // image_representation_pub_negative_TS_.shutdown();
    // image_representation_pub_AA_frequency_.shutdown();
    // image_representation_pub_AA_mat_.shutdown();
}

void ImageRepresentation::init(int width, int height)
{
    sensor_size_ = cv::Size(width, height);
    bSensorInitialized_ = true;
    printf("Sensor size: (%d x %d) /n", sensor_size_.width, sensor_size_.height);

    representation_TS_ = cv::Mat::zeros(sensor_size_, CV_32F);
    representation_AA_ = cv::Mat::zeros(sensor_size_, CV_8U);

    // Access to Eigen matrix is faster than cv::Mat
    TS_temp_map = Eigen::MatrixXd::Constant(sensor_size_.height, sensor_size_.width, -10);
    vEvents_.reserve(5000000);
}

void ImageRepresentation::GenerationLoop()
{
    const std::chrono::nanoseconds interval = std::chrono::nanoseconds(static_cast<long long>(1e9/generation_rate_hz_));
    timePoint next_wake_up_time = std::chrono::steady_clock::now();

    while (is_running) {
        createImageRepresentationAtTime(std::chrono::steady_clock::now());
        next_wake_up_time += interval;
        std::this_thread::sleep_until(next_wake_up_time);
    }
}

void ImageRepresentation::AA_thread(std::vector<esvo2_core::Event>::iterator &ptr_e, int distance, double external_t)
{
    timePoint external_sync_time(esvo2_core::secondsToTimePoint(external_t));

    representation_AA_ = cv::Mat::zeros(sensor_size_, CV_8U);   // for temporal stereo matching
    cv::Mat AA_frequency = cv::Mat::zeros(sensor_size_, CV_8U); // for point sampling

    std::vector<double> last_activity(x_patches_ * y_patches_, 0), event_activity(x_patches_ * y_patches_, 0),
        beta(x_patches_ * y_patches_, 0);
    std::vector<double> last_event_time(x_patches_ * y_patches_, 0);
    std::vector<bool> flag(x_patches_ * y_patches_, true);
    int flags = 0;
    double conv_thresh_ = 0.95; // convergence threshold
    std::vector<double> final_activity(x_patches_ * y_patches_, 0);
    std::vector<int> num(x_patches_ * y_patches_, 0);

    // std::vector<int> nums_temp(x_patches_ * y_patches_, 0);
    int nums_EQ = 0;
    // calculate the final activity by all events, also can be estimated by eq. 3 in the paper
    for (auto it = vEvents_.begin(); it != ptr_e; it++)
    {
        esvo2_core::Event e = *it;
        int y = e.y / (int)ceil((double)sensor_size_.height / (double)y_patches_);
        int x = e.x / (int)ceil((double)sensor_size_.width / (double)x_patches_);
        beta[y * x_patches_ + x] = 1 / (1 + final_activity[y * x_patches_ + x] *
                                                abs(esvo2_core::timePointToSec(e.timestamp) - last_event_time[y * x_patches_ + x])); // eq. 2
        if (y * x_patches_ + x >= x_patches_ * y_patches_)
            exit(-1);
        final_activity[y * x_patches_ + x] = beta[y * x_patches_ + x] * final_activity[y * x_patches_ + x] + 1; // eq. 1
        last_event_time[y * x_patches_ + x] = esvo2_core::timePointToSec(e.timestamp);
        // nums_temp[y * x_patches_ + x]++;
    }
    // for(int i = 0; i < x_patches_ * y_patches_; i++)
    // final_activity[i] = std::sqrt(1 / (0.01 / nums_temp[i]));  // eq. 3

    std::fill(beta.begin(), beta.end(), 0);
    std::fill(last_event_time.begin(), last_event_time.end(), 0);
    for (auto it = ptr_e; it != vEvents_.begin(); it--) // traverse events in reverse to accumulate the latest events
    {
        esvo2_core::Event e = *it;
        int y = e.y / (int)ceil((double)sensor_size_.height / (double)y_patches_);
        int x = e.x / (int)ceil((double)sensor_size_.width / (double)x_patches_);
        if (flag[y * x_patches_ + x] != true)
            continue;
        beta[y * x_patches_ + x] = 1 / (1 + event_activity[y * x_patches_ + x] *
                                                abs(esvo2_core::timePointToSec(e.timestamp) - last_event_time[y * x_patches_ + x]));       // eq. 2
        event_activity[y * x_patches_ + x] = beta[y * x_patches_ + x] * event_activity[y * x_patches_ + x] + 1; // eq. 1
        last_event_time[y * x_patches_ + x] = esvo2_core::timePointToSec(e.timestamp);
        AA_frequency.at<uchar>(e.y, e.x)++;
        num[y * x_patches_ + x]++;
        if (AA_frequency.at<uchar>(e.y, e.x) >= 1)
            representation_AA_.at<uchar>(e.y, e.x) = 255;
        if (num[y * x_patches_ + x] >= 10) // each patch is checked for convergence once every ten events accumulated
        {
            if (last_activity[y * x_patches_ + x] != 0)
            {
                if ((abs(event_activity[y * x_patches_ + x] - final_activity[y * x_patches_ + x])) < conv_thresh_)
                {
                    flag[y * x_patches_ + x] = false;
                    flags++;
                    if (flags == x_patches_ * y_patches_)
                        break;
                    else
                        continue;
                }
            }
            last_activity[y * x_patches_ + x] = event_activity[y * x_patches_ + x];
            num[y * x_patches_ + x] = 0;
        }
    }

    // distortion correction
    cv::remap(representation_AA_, representation_AA_, undistort_map1_, undistort_map2_, cv::INTER_LINEAR);

    // cv_bridge::CvImage cv_AA_frequency, cv_AA_mat;

    // ---Publishing AA_frequency AKA AA_Left
    // cv_bridge::CvImage cv_AA_frequency
    // cv_AA_frequency.encoding = "mono8";
    // cv_AA_frequency.image = AA_frequency.clone();
    // cv_AA_frequency.header.stamp = external_sync_time;
    // image_representation_pub_AA_frequency_.publish(cv_AA_frequency.toImageMsg());
    std::shared_ptr<cv::Mat> AA_freq = std::make_shared<cv::Mat>();
    *AA_freq = AA_frequency;
    AA_left_IR_to_Map_.add(AA_freq, external_sync_time);

    // ---Publishing AA_map
    // cv_AA_mat.encoding = "mono8";
    // cv_AA_mat.image = representation_AA_.clone();
    // cv_AA_mat.header.stamp = external_sync_time;
    // image_representation_pub_AA_mat_.publish(cv_AA_mat.toImageMsg());
    std::shared_ptr<cv::Mat> AA_map = std::make_shared<cv::Mat>();
    *AA_map = representation_AA_;
    multi_to_Map_.add<2>({AA_map, external_sync_time});

}

void ImageRepresentation::createImageRepresentationAtTime(const timePoint &external_sync_time)
{
    if (!bcreat_)
        return;
    else
        bcreat_ = false;
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (!bSensorInitialized_ || !bCamInfoAvailable_)
        return;

    // for AA generation
    cv::Mat filiter_image = cv::Mat::zeros(sensor_size_, CV_64F);
    cv::Mat rectangle_image = cv::Mat::zeros(cv::Size(80, 80), CV_8U);
    cv::Mat AA_frequency = cv::Mat::zeros(sensor_size_, CV_8U);

    if (representation_mode_ == Fast)
    {
        if (vEvents_.size() == 0)
            return;
        double external_t = esvo2_core::timePointToSec(external_sync_time);
        std::vector<esvo2_core::Event>::iterator ptr_e = EventVector_lower_bound(vEvents_, external_t);
        int distance = std::distance(vEvents_.begin(), ptr_e);

        if (is_left_) // generate AA and TS in parallel, just for left camera
        {
            std::thread thread0(&ImageRepresentation::AA_thread, this, std::ref(ptr_e), distance, external_t);
            representation_TS_.setTo(cv::Scalar(0));
            cv::Mat TS_img = cv::Mat::zeros(sensor_size_, CV_64F);

            // if the event rate is too high, we need to downsample the events
            // step = 1 indicates that we use all the events
            // double step = static_cast<double>(distance) / 90000.0;

            double step = 1;
            std::vector<esvo2_core::Event>::iterator it = vEvents_.begin();

            // generate TS map
            for (int i = 0; i < distance; i++)
            {
                int index = static_cast<int>(i * step);
                if (index > distance - 2)
                    break;
                esvo2_core::Event e = *(it + index);
                TS_temp_map(e.y, e.x) = esvo2_core::timePointToSec(e.timestamp) / decay_sec_;
            }

            cv::eigen2cv(TS_temp_map, representation_TS_);
            representation_TS_ = representation_TS_ - external_t / decay_sec_;
            cv::exp(representation_TS_, representation_TS_);

            TS_img = representation_TS_ * 255.0;
            TS_img.convertTo(TS_img, CV_8U);

            // distortion correction
            cv::remap(TS_img, TS_img, undistort_map1_, undistort_map2_, cv::INTER_LINEAR);

            // generate OS-TS
            cv::Mat TS_img_blur;
            cv::Mat OS_TS = TS_img.clone();
            cv::blur(TS_img, TS_img_blur, cv::Size(blur_size_, blur_size_));
            cv::Mat mask = (TS_img == 0);
            TS_img_blur.copyTo(OS_TS, mask);
            cv::medianBlur(TS_img, TS_img, 2 * median_blur_kernel_size_ + 1);

            // generate and publish gradient map in parallel
            if (thread_sobel.joinable())
                thread_sobel.join();
            negative_TS_img = cv::Mat::ones(sensor_size_, CV_8U);
            negative_TS_img = negative_TS_img * 255;
            negative_TS_img = negative_TS_img - OS_TS;

            // cv_bridge::CvImage cv_TS_image, cv_negative_TS_image;
            // dx and dy are published in sobel so idk why have the below
            // cv_dx_image.encoding = sensor_msgs::image_encodings::TYPE_16SC1;
            // cv_dy_image.encoding = sensor_msgs::image_encodings::TYPE_16SC1;
            // cv_dx_image.header.stamp = timePoint(external_t);
            // cv_dy_image.header.stamp = timePoint(external_t);
            
            thread_sobel = std::thread(&ImageRepresentation::sobel, this, external_t);

            // ---Publishing TS_left
            // cv_TS_image.encoding = "mono8";
            // cv_TS_image.header.stamp = timePoint(external_t);
            // cv_TS_image.image = TS_img.clone();
            // cv_TS_image.header.stamp = external_sync_time;
            // image_representation_pub_TS_.publish(cv_TS_image.toImageMsg());
            std::shared_ptr<cv::Mat> TS_left = std::make_shared<cv::Mat>();
            *TS_left = TS_img;
            // Can add same shared ptr to both because they treat it as a const in the callback functions I think
            multi_to_Track_.add<0>({TS_left, external_sync_time});
            multi_to_Map_.add<0>({TS_left, external_sync_time});

            // ---Publishing TS_neg
            // cv_negative_TS_image.encoding = "mono8";
            // cv_negative_TS_image.header.stamp = timePoint(external_t);
            // cv_negative_TS_image.image = negative_TS_img.clone();
            // cv_negative_TS_image.header.stamp = external_sync_time;
            // image_representation_pub_negative_TS_.publish(cv_negative_TS_image.toImageMsg());
            std::shared_ptr<cv::Mat> TS_neg = std::make_shared<cv::Mat>();
            *TS_neg = negative_TS_img;
            // Can add same shared ptr to both because they treat it as a const in the callback functions I think
            multi_to_Track_.add<1>({TS_neg, external_sync_time});
            multi_to_Map_.add<3>({TS_neg, external_sync_time});
            
            thread0.join();
        }
        else // generate TS, just for right camera
        {
            representation_TS_.setTo(cv::Scalar(0));
            cv::Mat TS_img = cv::Mat::zeros(sensor_size_, CV_64F);

            // double step = static_cast<double>(distance) / 90000.0;
            // if (step < 1)
            double step = 1;
            std::vector<esvo2_core::Event>::iterator it = vEvents_.begin();
            for (int i = 0; i < distance; i++)
            {
                int index = static_cast<int>(i * step);
                if (index > distance - 2)
                    break;
                esvo2_core::Event e = *(it + index);
                TS_temp_map(e.y, e.x) = esvo2_core::timePointToSec(e.timestamp) / decay_sec_;
            }
            cv::eigen2cv(TS_temp_map, representation_TS_);

            representation_TS_ = representation_TS_ - external_t / decay_sec_;
            cv::exp(representation_TS_, representation_TS_);
            TS_img = representation_TS_ * 255.0;
            TS_img.convertTo(TS_img, CV_8U);

            cv::remap(TS_img, TS_img, undistort_map1_, undistort_map2_, cv::INTER_LINEAR);

            cv::medianBlur(TS_img, TS_img, 2 * median_blur_kernel_size_ + 1);

           

            // ---Publishing TS_right
            // cv_bridge::CvImage cv_TS_image;
            // cv_TS_image.encoding = "mono8";
            // cv_TS_image.header.stamp = timePoint(external_t);
            // cv_TS_image.image = TS_img.clone();
            // image_representation_pub_TS_.publish(cv_TS_image.toImageMsg());
            std::shared_ptr<cv::Mat> TS_right = std::make_shared<cv::Mat>();
            *TS_right = TS_img;
            // Can add same shared ptr to both because they treat it as a const in the callback functions I think
            multi_to_Map_.add<1>({TS_right, external_sync_time});
        }

        clearEvents(distance, ptr_e);
    }
}

void ImageRepresentation::clearEvents(int distance, std::vector<esvo2_core::Event>::iterator ptr_e)
{
    if (vEvents_.size() > distance + 2)
        vEvents_.erase(vEvents_.begin(), ptr_e);
    else
        vEvents_.clear();
}

void ImageRepresentation::eventsCallback(const std::shared_ptr<esvo2_core::EventArray> &msg)
{
    TicToc t;
    std::lock_guard<std::mutex> lock(data_mutex_);
    double t1 = t.toc();
    if (!bSensorInitialized_)
        init(msg->width, msg->height);
    for (const esvo2_core::Event &e : msg->events)
    {
        if (e.x > sensor_size_.width || e.y > sensor_size_.height)
            continue;
        vEvents_.push_back(e);

        int i = vEvents_.size() - 2;
        while (i >= 0 && vEvents_[i].timestamp > e.timestamp)
        {
            vEvents_[i + 1] = vEvents_[i];
            i--;
        }
        vEvents_[i + 1] = e;
    }
    clearEventQueue();
    bcreat_ = true;
}

void ImageRepresentation::clearEventQueue()
{
    static constexpr size_t MAX_EVENT_QUEUE_LENGTH = 5000000;
    if (vEvents_.size() > MAX_EVENT_QUEUE_LENGTH)
    {
        size_t remove_events = vEvents_.size() - MAX_EVENT_QUEUE_LENGTH;
        vEvents_.erase(vEvents_.begin(), vEvents_.begin() + remove_events);
    }
}

void ImageRepresentation::sobel(double external_t)
{
    cv::Mat dx_result;
    cv::Mat dy_result;
    cv::Sobel(negative_TS_img, dx_result, CV_16SC1, 1, 0);
    cv::Sobel(negative_TS_img, dy_result, CV_16SC1, 0, 1);

    // Publishing dx
    // cv_dx_image.header.stamp = timePoint(external_t);
    // dx_image_pub_.publish(cv_dx_image.toImageMsg());
    std::shared_ptr<cv::Mat> dx_image = std::make_shared<cv::Mat>();
    *dx_image = std::move(dx_result);
    // Can add same shared ptr to both because they treat it as a const in the callback functions I think
    multi_to_Track_.add<2>({dx_image, esvo2_core::secondsToTimePoint(external_t)});
    multi_to_Map_.add<4>({dx_image, esvo2_core::secondsToTimePoint(external_t)});


    // Publishing dy
    // cv_dy_image.header.stamp = timePoint(external_t);
    // dy_image_pub_.publish(cv_dy_image.toImageMsg());
    std::shared_ptr<cv::Mat> dy_image = std::make_shared<cv::Mat>();
    *dy_image = std::move(dy_result);
    // Can add same shared ptr to both because they treat it as a const in the callback functions I think
    multi_to_Track_.add<3>({dy_image, esvo2_core::secondsToTimePoint(external_t)});
    multi_to_Map_.add<5>({dy_image, esvo2_core::secondsToTimePoint(external_t)});

}

bool ImageRepresentation::loadCalibInfo(const std::string& camera_yaml_path, bool &is_left)
{
    bCamInfoAvailable_ = false;
    std::string cam_calib_dir;
    // if (is_left)
    //     cam_calib_dir = cameraSystemDir + "/left.yaml";
    // else
    //     cam_calib_dir = cameraSystemDir + "/right.yaml";
    if (!fileExists(camera_yaml_path))
        return bCamInfoAvailable_;
    YAML::Node CamCalibInfo = YAML::LoadFile(camera_yaml_path);

    // load calib (left)
    size_t width = CamCalibInfo["image_width"].as<int>();
    size_t height = CamCalibInfo["image_height"].as<int>();
    std::string cameraNameLeft = CamCalibInfo["camera_name"].as<std::string>();
    std::string distortion_model = CamCalibInfo["distortion_model"].as<std::string>();
    std::vector<double> vD, vK, vRectMat, vP;
    std::vector<double> vT_right_left, vT_b_c;

    vD = CamCalibInfo["distortion_coefficients"]["data"].as<std::vector<double>>();
    vK = CamCalibInfo["camera_matrix"]["data"].as<std::vector<double>>();
    vRectMat = CamCalibInfo["rectification_matrix"]["data"].as<std::vector<double>>();
    vP = CamCalibInfo["projection_matrix"]["data"].as<std::vector<double>>();

    vT_right_left = CamCalibInfo["T_right_left"]["data"].as<std::vector<double>>();
    vT_b_c = CamCalibInfo["T_b_c"]["data"].as<std::vector<double>>();

    cv::Size sensor_size(width, height);
    camera_matrix_ = cv::Mat(3, 3, CV_64F);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            camera_matrix_.at<double>(cv::Point(i, j)) = vK[i + j * 3];

    distortion_model_ = distortion_model;
    dist_coeffs_ = cv::Mat(vD.size(), 1, CV_64F);
    for (int i = 0; i < vD.size(); i++)
        dist_coeffs_.at<double>(i) = vD[i];

    if (bUseStereoCam_)
    {
        rectification_matrix_ = cv::Mat(3, 3, CV_64F);
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                rectification_matrix_.at<double>(cv::Point(i, j)) = vRectMat[i + j * 3];

        projection_matrix_ = cv::Mat(3, 4, CV_64F);
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 3; j++)
                projection_matrix_.at<double>(cv::Point(i, j)) = vP[i + j * 4];

        if (distortion_model_ == "equidistant")
        {
            cv::fisheye::initUndistortRectifyMap(camera_matrix_, dist_coeffs_, rectification_matrix_,
                                                 projection_matrix_, sensor_size, CV_32FC1, undistort_map1_,
                                                 undistort_map2_);
            bCamInfoAvailable_ = true;
            printf("Camera information is loaded (Distortion model %s \n).", distortion_model_.c_str());
        }
        else if (distortion_model_ == "plumb_bob")
        {
            cv::initUndistortRectifyMap(camera_matrix_, dist_coeffs_, rectification_matrix_, projection_matrix_,
                                        sensor_size, CV_32FC1, undistort_map1_, undistort_map2_);
            bCamInfoAvailable_ = true;
            printf("Camera information is loaded (Distortion model %s \n).", distortion_model_.c_str());
        }
        else
        {
            std::cerr<<"Distortion model <<"<<distortion_model_.c_str()<<"is not supported."<<std::endl;

            return bCamInfoAvailable_;
        }

        /* pre-compute the undistorted-rectified look-up table */
        precomputed_rectified_points_ = Eigen::Matrix2Xd(2, sensor_size.height * sensor_size.width);
        // raw coordinates
        cv::Mat_<cv::Point2f> RawCoordinates(1, sensor_size.height * sensor_size.width);
        for (int y = 0; y < sensor_size.height; y++)
        {
            for (int x = 0; x < sensor_size.width; x++)
            {
                int index = y * sensor_size.width + x;
                RawCoordinates(index) = cv::Point2f((float)x, (float)y);
            }
        }
        // undistorted-rectified coordinates
        cv::Mat_<cv::Point2f> RectCoordinates(1, sensor_size.height * sensor_size.width);
        if (distortion_model_ == "plumb_bob")
        {
            cv::undistortPoints(RawCoordinates, RectCoordinates, camera_matrix_, dist_coeffs_, rectification_matrix_,
                                projection_matrix_);
            printf("Undistorted-Rectified Look-Up Table with Distortion model: %s \n", distortion_model_.c_str());
        }
        else if (distortion_model_ == "equidistant")
        {
            cv::fisheye::undistortPoints(RawCoordinates, RectCoordinates, camera_matrix_, dist_coeffs_,
                                         rectification_matrix_, projection_matrix_);
            printf("Undistorted-Rectified Look-Up Table with Distortion model: %s \n", distortion_model_.c_str());
        }
        else
        {
            printf("Unknown distortion model is provided.\n");
            return bCamInfoAvailable_;
        }
        // load look-up table
        for (size_t i = 0; i < sensor_size.height * sensor_size.width; i++)
        {
            precomputed_rectified_points_.col(i) =
                Eigen::Matrix<double, 2, 1>(RectCoordinates(i).x, RectCoordinates(i).y);
        }
        printf("Undistorted-Rectified Look-Up Table has been computed.\n");
    }
    else
    {
        // TODO: calculate undistortion map
        bCamInfoAvailable_ = true;
    }
    return bCamInfoAvailable_;
}

bool ImageRepresentation::fileExists(const std::string &filename)
{
    std::ifstream file(filename);
    return file.good();
}

} // namespace image_representation
