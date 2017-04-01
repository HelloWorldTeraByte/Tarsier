#ifndef _TXTPROC_H_
#define _TXTPROC_H_

#include <opencv2/core/core.hpp> 

#define LOWEST_LETTER_AREA 2
#define X_EPSILON 2
#define SPACE_WIDTH 6
#define APPROX_POLY_EP 1

std::vector<cv::Rect> getLetters(cv::Mat inMat, std::vector<unsigned char> &spacePos);
std::vector<cv::Rect> getLetters(cv::Mat inMat);
std::vector<cv::Rect> getWords(cv::Mat inMat);
int getLetterCount(cv::Mat inMat);
bool sortByX(cv::Rect & a, cv::Rect &b);

#endif

