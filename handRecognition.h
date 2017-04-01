#ifndef _HANDRECOGNITION_H
#define _HANDRECOGNITION_H

#include <opencv2/core/core.hpp> 

cv::Point pointingLoc(cv::Mat bigMat, cv::MatND hist);
cv::MatND calibrateToColor(cv::Mat bigMat, bool bCalibrate);
cv::Scalar getHSVAtPoint(cv::Mat bigMat, cv::Point ptToProc);
#endif
