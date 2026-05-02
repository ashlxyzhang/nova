#ifndef image_representation_H_
#define image_representation_H_

// From STD Library
#include <algorithm>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

// FROM SLAM
#include "data_passing.hh"
#include "multi_data_passing.hh"
#include "esvo2_core/tools/types.h"
#include "image_representation/TicToc.h"

// From Dependencies
#include <yaml-cpp/yaml.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <Eigen/Eigen>

namespace image_representation
{
using timePoint = std::chrono::time_point<std::chrono::steady_clock>;
using EventQueue = std::deque<esvo2_core::Event>;

struct ROSTimeCmp
{
        bool operator()(const timePoint &a, const timePoint &b) const
        {
            // return a.toNSec() < b.toNSec();
            return a < b;
        }
};
using GlobalEventQueue = std::map<timePoint, esvo2_core::Event, ROSTimeCmp>;

inline static EventQueue::iterator EventBuffer_lower_bound(EventQueue &eb, timePoint &t)
{
    return std::lower_bound(eb.begin(), eb.end(), t,
                            [](const esvo2_core::Event &e, const timePoint &t) { return e.timestamp < t; });
}

inline static EventQueue::iterator EventBuffer_upper_bound(EventQueue &eb, timePoint &t)
{
    return std::upper_bound(eb.begin(), eb.end(), t,
                            [](const timePoint &t, const esvo2_core::Event &e) { return t < e.timestamp; });
}

inline static std::vector<esvo2_core::Event>::iterator EventVector_lower_bound(std::vector<esvo2_core::Event> &ev, double &t)
{
    return std::lower_bound(ev.begin(), ev.end(), t, [](const esvo2_core::Event &e, const double &t) { return esvo2_core::timePointToSec(e.timestamp) < t; });
}

class ImageRepresentation
{
    public:
        ImageRepresentation(std::atomic<bool> &is_running_, const YAML::Node &config,  
            const std::string& camera_yaml_path,
            MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat>& multi_to_Track, 
            MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat>& multi_to_Map,
            DataPassingDeque<cv::Mat>& AA_left_IR_to_Map
        );
        virtual ~ImageRepresentation();

        static bool compare_time(const esvo2_core::Event &e, const double reference_time)
        {
            return reference_time < esvo2_core::timePointToSec(e.timestamp);
        }

        // callbacks
        void eventsCallback(const std::shared_ptr<esvo2_core::EventArray> &msg);

    private:
        // message passing stuff
        // TSleft, TSnegative, dx, dy; All left only
        MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat>& multi_to_Track_;
        // TSleft, TSright, AA MAP, TS neg, dx, dy; ALL left except for TSright!
        MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat>& multi_to_Map_;
        DataPassingDeque<cv::Mat>& AA_left_IR_to_Map_; 

        // Running variable
        std::atomic<bool> &is_running;

        // configuration variables struct
        YAML::Node config_;

        // core
        void init(int width, int height);
        // Support: TS, AA, negative_TS, negative_TS_dx, negative_TS_dy
        void createImageRepresentationAtTime(const timePoint &external_sync_time);
        void GenerationLoop();

       

        // utils
        void clearEventQueue();
        bool loadCalibInfo(const std::string &camera_yaml_path, bool &is_left);
        void clearEvents(int distance, std::vector<esvo2_core::Event>::iterator ptr_e);

        void AA_thread(std::vector<esvo2_core::Event>::iterator &ptr_e, int distance, double external_t);
        void sobel(double external_t);
        bool fileExists(const std::string &filename);
        // tests

        // calibration parameters
        cv::Mat camera_matrix_, dist_coeffs_;
        cv::Mat rectification_matrix_, projection_matrix_;
        std::string distortion_model_;
        cv::Mat undistort_map1_, undistort_map2_;
        Eigen::Matrix2Xd precomputed_rectified_points_;

        bool left_;
        cv::Mat negative_TS_img;
        std::thread thread_sobel;

        // online parameters
        bool bCamInfoAvailable_;
        bool bUse_Sim_Time_;
        cv::Size sensor_size_;
        timePoint sync_time_;
        bool bSensorInitialized_;

        // offline parameters TODO
        double decay_ms_;
        bool ignore_polarity_;
        int median_blur_kernel_size_;
        int blur_size_;
        int max_event_queue_length_;
        int events_maintained_size_;

        // containers
        EventQueue events_;

        std::vector<esvo2_core::Event> vEvents_;

        cv::Mat representation_TS_;
        cv::Mat representation_AA_;

        Eigen::MatrixXd TS_temp_map;

        // for rectify
        cv::Mat undistmap1_, undistmap2_;
        bool is_left_, bcreat_;

        // thread mutex
        std::mutex data_mutex_;

        enum RepresentationMode
        {
            Linear_TS, // 0
            AA2,       // 1
            Fast       // 2
        } representation_mode_;

        // parameters
        bool bUseStereoCam_;
        double decay_sec_; // TS param
        int generation_rate_hz_;
        int x_patches_, y_patches_;
        // std::vector<Event>::iterator ptr_e_;

        // calib info
        std::string calibInfoDir_;
        std::vector<cv::Point> trapezoid_;
};
} // namespace image_representation
#endif // image_representation_H_