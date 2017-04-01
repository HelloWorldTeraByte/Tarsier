#include <iostream>
#include <signal.h>
#include <opencv2/core/core.hpp> 
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include "defs.h"
#include "handRecognition.h"
#include "txtProc.h"

using namespace cv;

static volatile int keepRunning = 1;

void runHandler(int dummy) 
{
    keepRunning = 0;
}

bool SortByYInverse(const cv::Point &a, const cv::Point &b)
{
    return a.y < b.y;
}

bool SortByY(const cv::Point &a, const cv::Point &b)
{
    return a.y > b.y;
}

//int closestToPoint(std::vector<int> const &vec, int value)
//{
//    auto const it = std::lower_bound(vec.begin(), vec.end(), value);
//    if (it == vec.end()) { return -1; }
//
//    return *it;
//}

void warnOffTheLine(std::vector<int>::iterator &linePoint, cv::Point &centerPoint)
{
    //Remeber to *the iterator
//    int diff = *linePoint - centerPoint.y;
//
//    if (diff > 5 && diff < 10)
//        cout << "Good" << endl;
//    else if( diff > 12)
//        cout << "Move down" << endl;
//    else if(diff < 4)
//        cout << "Move up" << endl;
}

void fixSkew(cv::Mat inMat)
{
    cv::Size size = inMat.size();
    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(inMat, lines, 1, CV_PI / 180, 100, size.width / 2.f, 20);

    cv::Mat disp_lines(size, CV_8UC1, cv::Scalar(0, 0, 0));
    double angle = 0.;
    unsigned long nb_lines = lines.size();
    for (unsigned i = 0; i < nb_lines; ++i) {
        cv::line(disp_lines, cv::Point(lines[i][0], lines[i][1]),
                 cv::Point(lines[i][2], lines[i][3]), cv::Scalar(255, 0, 0));
        angle += atan2((double) lines[i][3] - lines[i][1],
                       (double) lines[i][2] - lines[i][0]);
    }
    angle /= nb_lines; // mean angle, in radians.k


    std::vector<cv::Point> points;
    cv::Mat_<uchar>::iterator it = inMat.begin<uchar>();
    cv::Mat_<uchar>::iterator end = inMat.end<uchar>();
    for (; it != end; ++it)
        if (*it)
            points.push_back(it.pos());

    cv::RotatedRect box = cv::minAreaRect(cv::Mat(points));

    cv::Mat rotMat = cv::getRotationMatrix2D(box.center, angle * 180 / CV_PI, 1);
    cv::Mat rotatedMat;
    cv::warpAffine(inMat, rotatedMat, rotMat, size, cv::INTER_CUBIC);
    //imshow("as", rotatedMat);
}

void setVideoSettings(cv::VideoCapture &cap)
{
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 720);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 480);
} 

int main(int argc, char **argv)
{
    //cv::VideoCapture cap("http://192.168.1.5:8080/?dummy=param.mjpg"); 
    cv::VideoCapture cap(1); 
    setVideoSettings(cap);

    if(!cap.isOpened()) {
        perror("ERROR: Cannot open the Video!");
        return -1;
    }

    signal(SIGINT, runHandler);

    cv::Mat bigMat;
    cv::MatND hist;
    bool bCalibrate = false;

    while(keepRunning) {
        
        int64 e1 = getCPUTickCount();

        bool bSuccess = cap.read(bigMat);
        if(!bSuccess) {
            std::cerr << "ERROR: Cannot read from the video!" << std::endl;
            return -1;
        }

        hist = calibrateToColor(bigMat, bCalibrate);

        //Top left and bottom right points for the cropping from the bigMat
        cv::Point tlPoint;
        cv::Point brPoint;

        //The center point is set by the users click after this, and its not really the center point after this
        cv::Point centerPoint;
        centerPoint.x = bigMat.cols / 2;
        centerPoint.y = bigMat.rows / 2;

        cv::Mat grayFrame, adaptMat, inputFrame;

        //PointingLoc() function corrupts the image
        cv::Mat bigMatPointProc = bigMat.clone();
        cv::Point pointingLoct = pointingLoc(bigMatPointProc, hist);
        if (pointingLoct.x < bigMat.cols && pointingLoct.y - OFFSET_HAND < bigMat.rows && pointingLoct.y - OFFSET_HAND > 0 && pointingLoct.x > 0) {
            centerPoint.y = pointingLoct.y - OFFSET_HAND;
            centerPoint.x = pointingLoct.x;
        }


        cv::Mat bigMatProc = bigMat.clone();
        cvtColor(bigMat, bigMatProc, cv::COLOR_BGR2GRAY);
        cv::threshold(~bigMatProc, bigMatProc, 0, 255, CV_THRESH_OTSU + CV_THRESH_BINARY);

        int morpType = MORPH_RECT;

        int dilateMorpSize = 3;
        Mat elementDilate = getStructuringElement(morpType,
                Size(10 * dilateMorpSize + 1, 2 * dilateMorpSize),
                Point(dilateMorpSize, dilateMorpSize)); //Higher width than height for a horizontal morph dilation

        cv::dilate(bigMatProc, bigMatProc, elementDilate);

        //fixSkew(bigMatProc);

        Mat horizontal = bigMatProc;

        //Just mad thingz
        int horizontalsize = horizontal.cols / 5;
        Mat horizontalStructure = getStructuringElement(MORPH_RECT, Size(horizontalsize, 1));

        erode(horizontal, horizontal, horizontalStructure, Point(-1, -1));
        dilate(horizontal, horizontal, horizontalStructure, Point(-1, -1));


        cv::Mat bigMatCont = bigMatProc.clone();
        std::vector<std::vector<cv::Point> > linesContours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(bigMatCont, linesContours, hierarchy, CV_RETR_EXTERNAL,
                CV_CHAIN_APPROX_SIMPLE);

        std::vector<std::vector<cv::Point> > contoursPoly(linesContours.size());
        std::vector<cv::RotatedRect> foundLines(linesContours.size());
        std::vector<cv::Point> linePoints;
        std::vector<int> linePointsInt;

        for (int i = 0; i < linesContours.size(); i++)
            foundLines[i] = minAreaRect(Mat(linesContours[i]));

        for (int i = 0; i < linesContours.size(); i++) {
            Point2f rect_points[4];
            foundLines[i].points(rect_points);
            std::vector<Point2f> rectPoints(rect_points,
                    rect_points + sizeof rect_points / sizeof rect_points[0]);
            sort(rectPoints.begin(), rectPoints.end(), SortByY);
            cv::Point rectPointss = rectPoints.at(0);
            rectPointss.y = rectPoints.at(0).y + rectPoints.at(1).y;
            rectPointss.y /= 2;
            linePoints.push_back(rectPointss);
        }

        sort(linePoints.begin(), linePoints.end(), SortByYInverse);

        for (unsigned int i = 0; i < linePoints.size(); i++) {
            linePointsInt.push_back(linePoints.at(i).y);
        }

        std::vector<int>::iterator closestPoint;
        closestPoint = std::lower_bound(linePointsInt.begin(), linePointsInt.end(), centerPoint.y);

#ifdef UI_ON
        imshow("debug", bigMatProc);
#endif

        //TODO: Better word height recognition
        //Automatic height recognition
        if (bigMatProc.at<uchar>(centerPoint.y, centerPoint.x) == 255) {

            tlPoint = centerPoint;
            brPoint = centerPoint;
            if (bigMatProc.at<uchar>(centerPoint.y, centerPoint.x) == 255) {
                while (1) {
                    if (tlPoint.y <= 2)
                        break;

                    //If there is recognised text under the cursor increse the top corner.y by x
                    if (bigMatProc.at<uchar>(tlPoint.y, centerPoint.x) == 255)
                        tlPoint.y -= 5;
                    else {
                        tlPoint.y -= 4;
                        break;
                    }

                }
                while (1) {
                    if (bigMatProc.rows - brPoint.y <= 2)
                        break;

                    if (bigMatProc.at<uchar>(brPoint.y, centerPoint.x) == 255)
                        brPoint.y += 5;
                    else {
                        brPoint.y += 4;
                        break;
                    }

                }

            }

        }else {
            tlPoint.x = centerPoint.x + -200;
            if (tlPoint.x < 0)
                tlPoint.x = 0;

            tlPoint.y = centerPoint.y + -20;
            if (tlPoint.y < 0)
                tlPoint.y = 0;

            brPoint.x = centerPoint.x + 200;
            if (brPoint.x > bigMat.cols)
                brPoint.x = bigMat.cols;

            brPoint.y = centerPoint.y + 20;
            if (brPoint.y > bigMat.rows)
                brPoint.y = bigMat.rows;
        } //If else there are no characters recognised under the point make a set amount ROI

        //Make sure that these are point within the image ie. valid
        //if not fit them in the image
        tlPoint.x = centerPoint.x + -200;
        if (tlPoint.x < 0)
            tlPoint.x = 0;

        brPoint.x = centerPoint.x + 200;
        if (brPoint.x > bigMat.cols)
            brPoint.x = bigMat.cols;

        if (tlPoint.y < 0)
            tlPoint.y = 0;

        if (brPoint.y > bigMat.rows)
            brPoint.y = bigMat.rows;


        //Warn the user if the finger is moving off the line
        if (closestPoint != linePointsInt.end())
            warnOffTheLine(closestPoint, centerPoint);


        //Crop the big image down to an interested section
        inputFrame = bigMat(cv::Rect(tlPoint, brPoint));

        cvtColor(inputFrame, grayFrame, cv::COLOR_BGR2GRAY);
        cv::threshold(~grayFrame, adaptMat, 0, 255, CV_THRESH_OTSU + CV_THRESH_BINARY);

        //getWords corrupts the image so make a deep copy
        cv::Mat matLetProc, boxMat;
        matLetProc = adaptMat.clone();
        boxMat = inputFrame.clone();

        //Find the word from the image and store it in array of rectangles
        std::vector<cv::Rect> foundWords = getWords(matLetProc);

        //Center point for processing, this point is used to see which letters the use is pointing at
        cv::Point ptToProc;

        ptToProc.x = boxMat.cols / 2;
        ptToProc.y = boxMat.rows / 2;

        //Draw the processing point in a crosshair fashion in the boxmat
        cv::line(boxMat, cv::Point(ptToProc.x + 10, ptToProc.y),
                cv::Point(ptToProc.x - 10, ptToProc.y), cv::Scalar(0, 0, 255), 1, 8, 0);
        cv::line(boxMat, cv::Point(ptToProc.x, ptToProc.y + 10),
                cv::Point(ptToProc.x, ptToProc.y - 10), cv::Scalar(0, 0, 255), 1, 8, 0);


        cv::Mat oneWord;
        cv::Mat wordMat;
        bool showWords = true;
        for (unsigned int i = 0; i < foundWords.size(); i++) {

            if (ptToProc.x > foundWords.at(i).tl().x && ptToProc.x < foundWords.at(i).br().x) {
                if ((foundWords.at(i) & cv::Rect(0, 0, grayFrame.cols, grayFrame.rows)) ==
                        foundWords.at(i))
                    oneWord = grayFrame(foundWords.at(i));

                if (!oneWord.empty()) {

                    //cv::Mat oneWordProc = adaptMat(foundWords.at(i));
                    wordMat = oneWord.clone();

                    cv::rectangle(boxMat, foundWords.at(i), cv::Scalar(255, 33, 33), 2, 8, 0);

                }
            }
            if (showWords)
                cv::rectangle(boxMat, foundWords.at(i), cv::Scalar(0, 200, 0), 1, 8, 0);

        }

        //Draw the center point in a crosshair fashion
        cv::line(bigMat, cv::Point(centerPoint.x + 10, centerPoint.y),
                cv::Point(centerPoint.x - 10, centerPoint.y), cv::Scalar(0, 0, 255), 3, 8, 0);
        cv::line(bigMat, cv::Point(centerPoint.x, centerPoint.y + 10),
                cv::Point(centerPoint.x, centerPoint.y - 10), cv::Scalar(0, 0, 255), 3, 8, 0);

        //Draw the closest line where the users hand is at
        if (closestPoint != linePointsInt.end())
            cv::line(bigMat, cv::Point(0, *closestPoint), cv::Point(bigMat.cols, *closestPoint), cv::Scalar(0, 255, 0), 1, 8, 0);

        //Show windows if the detected OS is Linux

#ifdef UI_ON
        cv::imshow("boxMat", boxMat);
        cv::imshow("BigImage", bigMat);
        cv::waitKey(100);
#endif
        int64 e2 = getCPUTickCount();
        float times = (float)((e2 - e1)/ getTickFrequency());
        std::cout << "TIME: " <<  times << std::endl;



    }

    return 0;
}
