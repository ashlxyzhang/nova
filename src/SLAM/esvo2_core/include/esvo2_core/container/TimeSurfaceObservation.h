#ifndef ESVO2_CORE_CONTAINER_TIMESURFACEOBSERVATION_H
#define ESVO2_CORE_CONTAINER_TIMESURFACEOBSERVATION_H

#include <cv_bridge/cv_bridge.h>
#include <kindr/minimal/quat-transformation.h>

#include <opencv2/core/eigen.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <esvo2_core/tools/TicToc.h>
#include <esvo2_core/tools/utils.h>

//#define TIME_SURFACE_OBSERVATION_LOG
namespace esvo2_core
{
using namespace tools;
namespace container
{
struct TimeSurfaceObservation
{
  TimeSurfaceObservation(
    cv_bridge::CvImagePtr &left,
    cv_bridge::CvImagePtr &right,
    Transformation &tr,
    size_t id,
    bool bCalcTsGradient = false)
    : tr_(tr),
      id_(id)
  {
    cv::cv2eigen(left->image, TS_left_);
    cv::cv2eigen(right->image, TS_right_);

    if (bCalcTsGradient)
    {
#ifdef TIME_SURFACE_OBSERVATION_LOG
      TicToc tt;
      tt.tic();
#endif
      // cv::Mat cv_dTS_du_left_, cv_dTS_dv_left_;
      cv::Sobel(left->image, cv_dTS_du_left_, CV_64F, 1, 0);
      cv::Sobel(left->image, cv_dTS_dv_left_, CV_64F, 0, 1);
      cv::cv2eigen(cv_dTS_du_left_, dTS_du_left_);
      cv::cv2eigen(cv_dTS_dv_left_, dTS_dv_left_);
#ifdef TIME_SURFACE_OBSERVATION_LOG
      LOG(INFO) << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@Sobel computation (" << id_ << ") takes " << tt.toc() << " ms.";
#endif
    }
    bSubNegaTS_ = false;
  }

  // override version without transformation and with negative TS.
  TimeSurfaceObservation(
    cv_bridge::CvImagePtr &left,
    cv_bridge::CvImagePtr &negative,
    cv_bridge::CvImagePtr &negative_dx,
    cv_bridge::CvImagePtr &negative_dy,
    size_t id,
    bool bCalcTsGradient = false)
    : id_(id)
  {
    cvImagePtr_left_ = left;
    // // used for multi-level pyramid
    // cv_TS_negative_left_ = negative->image;
    // cv_dTS_negative_du_left_ = negative_dx->image;
    // cv_dTS_negative_dv_left_ = negative_dy->image;

    cv::cv2eigen(left->image, TS_left_);
    cv::cv2eigen(negative_dx->image, dTS_negative_du_left_);
    cv::cv2eigen(negative_dy->image, dTS_negative_dv_left_);
    cv::cv2eigen(negative->image, TS_negative_left_);
    
    if (bCalcTsGradient)
    {
#ifdef TIME_SURFACE_OBSERVATION_LOG
      TicToc tt;
      tt.tic();
#endif
      cv::Sobel(left->image, cv_dTS_du_left_, CV_64F, 1, 0);
      cv::Sobel(left->image, cv_dTS_dv_left_, CV_64F, 0, 1);
      cv::cv2eigen(cv_dTS_du_left_, dTS_du_left_);
      cv::cv2eigen(cv_dTS_dv_left_, dTS_dv_left_);
#ifdef TIME_SURFACE_OBSERVATION_LOG
      LOG(INFO) << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@Sobel computation (" << id_ << ") takes " << tt.toc() << " ms.";
#endif
    }
    bSubNegaTS_ = true;
  }

  // override version without initializing the transformation in the constructor.
  TimeSurfaceObservation(
    cv_bridge::CvImagePtr &left,
    cv_bridge::CvImagePtr &right,
    size_t id,
    bool bCalcTsGradient = false)
    : id_(id)
  {
    cvImagePtr_left_ = left;
    cvImagePtr_right_ = right;
    cv::cv2eigen(left->image, TS_left_);
    cv::cv2eigen(right->image, TS_right_);
    
    if (bCalcTsGradient)
    {
#ifdef TIME_SURFACE_OBSERVATION_LOG
      TicToc tt;
      tt.tic();
#endif
      // cv::Mat cv_dTS_du_left_, cv_dTS_dv_left_;
      cv::Mat cv_dTS_du_right, cv_dTS_dv_right;
      cv::Sobel(left->image, cv_dTS_du_left_, CV_64F, 1, 0);
      cv::Sobel(left->image, cv_dTS_dv_left_, CV_64F, 0, 1);

      cv::cv2eigen(cv_dTS_du_left_, dTS_du_left_);
      cv::cv2eigen(cv_dTS_dv_left_, dTS_dv_left_);

#ifdef TIME_SURFACE_OBSERVATION_LOG
      LOG(INFO) << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@Sobel computation (" << id_ << ") takes " << tt.toc() << " ms.";
#endif
    }
    bSubNegaTS_ = true;
  }

  TimeSurfaceObservation(
    cv_bridge::CvImagePtr &left,
    cv_bridge::CvImagePtr &right,
    cv_bridge::CvImagePtr &AA_map,
    cv_bridge::CvImagePtr &negative,
    cv_bridge::CvImagePtr &negative_dx,
    cv_bridge::CvImagePtr &negative_dy,
    size_t id,
    bool bCalcTsGradient = false)
    : id_(id)
  {
    cvImagePtr_left_ = left;
    cvImagePtr_right_ = right;
    cvImagePtr_AA_map_ = AA_map;

    cv::cv2eigen(left->image, TS_left_);
    cv::cv2eigen(right->image, TS_right_);
    cv::cv2eigen(AA_map->image, AA_map_);
    cv::cv2eigen(negative_dx->image, dTS_negative_du_left_);
    cv::cv2eigen(negative_dy->image, dTS_negative_dv_left_);
    cv::cv2eigen(negative->image, TS_negative_left_);
    // cv::imshow("mapping_negative", negative->image);
    // cv::waitKey(1);
    if (bCalcTsGradient)
    {
#ifdef TIME_SURFACE_OBSERVATION_LOG
      TicToc tt;
      tt.tic();
#endif
      cv::Sobel(left->image, cv_dTS_du_left_, CV_64F, 1, 0);
      cv::Sobel(left->image, cv_dTS_dv_left_, CV_64F, 0, 1);
      cv::cv2eigen(cv_dTS_du_left_, dTS_du_left_);
      cv::cv2eigen(cv_dTS_dv_left_, dTS_dv_left_);
      cv::Sobel(right->image, cv_dTS_du_right_, CV_64F, 1, 0);
      cv::Sobel(right->image, cv_dTS_dv_right_, CV_64F, 0, 1);
      cv::cv2eigen(cv_dTS_du_right_, dTS_du_right_);
      cv::cv2eigen(cv_dTS_dv_right_, dTS_dv_right_);

#ifdef TIME_SURFACE_OBSERVATION_LOG
      LOG(INFO) << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@Sobel computation (" << id_ << ") takes " << tt.toc() << " ms.";
#endif
    }
    bSubNegaTS_ = false;
  }

  TimeSurfaceObservation()
  {};

  inline bool isEmpty()
  {
    if(TS_left_.rows() == 0 || TS_left_.cols() == 0 || TS_right_.rows() == 0 || TS_right_.cols() == 0)
      return true;
    else
      return false;
  }

  inline void setTransformation(Transformation &tr)
  {
    tr_ = tr;
  }

  inline void setOriTransformation(Transformation &tr)
  {
    tr_ori_ = tr;
  }

  inline void GaussianBlurTS(size_t kernelSize)
  {
    cv::Mat mat_left_, mat_right_, mat_last_;
    cv::GaussianBlur(cvImagePtr_left_->image, mat_left_,
                     cv::Size(kernelSize, kernelSize), 0.0);
    cv::GaussianBlur(cvImagePtr_right_->image, mat_right_,
                     cv::Size(kernelSize, kernelSize), 0.0);
    cv::cv2eigen(mat_left_, TS_left_);
    cv::cv2eigen(mat_right_, TS_right_);

    if(TS_last_.rows() != 0 && TS_last_.cols() != 0 && TS_last_.rows() != 0 && TS_last_.cols() != 0)
    {
      cv::GaussianBlur(cvImagePtr_last_->image, mat_last_,
                    cv::Size(kernelSize, kernelSize), 0.0);
      cv::cv2eigen(mat_last_, TS_last_);
    }
  }

  inline void getTimeSurfaceNegative(size_t kernelSize)
  {
    Eigen::MatrixXd ceilMat(TS_left_.rows(), TS_left_.cols());
    ceilMat.setConstant(255.0);
    if (kernelSize > 0)
    {
      cv::Mat mat_left_;
      cv::GaussianBlur(cvImagePtr_left_->image, mat_left_,
                       cv::Size(kernelSize, kernelSize), 0.0);
      cv::cv2eigen(mat_left_, TS_blurred_left_);
      TS_negative_left_ = ceilMat - TS_blurred_left_;
    }
    else
    {
      TS_negative_left_ = ceilMat - TS_left_;
    }
  }

  inline void computeTsNegativeGrad()
  {
    cv::Mat cv_TS_flipped_left;
    cv::eigen2cv(TS_negative_left_, cv_TS_flipped_left);

    cv::Mat cv_dFlippedTS_du_left, cv_dFlippedTS_dv_left;
    cv::Sobel(cv_TS_flipped_left, cv_dFlippedTS_du_left, CV_64F, 1, 0);
    cv::Sobel(cv_TS_flipped_left, cv_dFlippedTS_dv_left, CV_64F, 0, 1);

    cv::cv2eigen(cv_dFlippedTS_du_left, dTS_negative_du_left_);
    cv::cv2eigen(cv_dFlippedTS_dv_left, dTS_negative_dv_left_);
  }

  Eigen::MatrixXd TS_left_, TS_right_, TS_last_, AA_map_, TS_last_du, TS_last_dv;
  Eigen::MatrixXd TS_blurred_left_;
  Eigen::MatrixXd TS_negative_left_;
  cv_bridge::CvImagePtr cvImagePtr_left_, cvImagePtr_right_, cvImagePtr_last_, cvImagePtr_AA_map_;
  Transformation tr_, tr_last_, tr_ori_;
  Eigen::MatrixXd dTS_du_left_, dTS_dv_left_, dTS_du_right_, dTS_dv_right_;
  cv::Mat cv_dTS_du_left_, cv_dTS_dv_left_, cv_dTS_du_right_, cv_dTS_dv_right_;
  Eigen::MatrixXd dTS_negative_du_left_, dTS_negative_dv_left_;
  cv::Mat cv_TS_negative_left_, cv_dTS_negative_du_left_, cv_dTS_negative_dv_left_;
  size_t id_;
  cv::Mat x_grad_, y_grad_;
  bool bSubNegaTS_;
  EventQueue events_;
};

using TimeSurfaceHistory = std::map<timePoint, TimeSurfaceObservation>;
using StampedTimeSurfaceObs = std::pair<timePoint, TimeSurfaceObservation>;
using constStampedTimeSurfaceObs = std::pair<const timePoint, TimeSurfaceObservation>;
}
}

#endif //ESVO2_CORE_CONTAINER_TIMESURFACEOBSERVATION_H
