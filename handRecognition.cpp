#include <string>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "defs.h"
#include "handRecognition.h"

using namespace cv;

cv::MatND calibrateToColor(cv::Mat bigMat, bool bCalibrate)
{
    cv::Mat ROI;
    if(bCalibrate) {
        cv::Point ptToProc;

        int offset = 50;
        for(int i = 0; i <= 4; i++) {
            ptToProc.x = bigMat.cols /2;
            ptToProc.y = bigMat.rows /2;

            switch (i)
            {
                case 0:
                    ptToProc.x = bigMat.cols /2;
                    ptToProc.y = bigMat.rows /2;
                    break;
                case 1:
                    ptToProc.x -= offset;
                    ptToProc.y -= offset;
                    break;
                case 2:
                    ptToProc.x += offset;
                    ptToProc.y -= offset;
                    break;
                case 3:
                    ptToProc.x -= offset;
                    ptToProc.y += offset;
                    break;
                case 4:
                    ptToProc.x += offset;
                    ptToProc.y += offset;
                    break;
                default:
                    ptToProc.x = bigMat.cols /2;
                    ptToProc.y = bigMat.rows /2;
            }

            cv::Rect tRect(cv::Point(ptToProc.x -8, ptToProc.y -8), cv::Point( ptToProc.x +8, ptToProc.y +8));
            ROI.push_back(bigMat(tRect));
            cv::rectangle(bigMat, cv::Point(ptToProc.x-16/2,ptToProc.y-16/2), cv::Point(ptToProc.x+16/2,ptToProc.y+16/2), cv::Scalar(255,0,0), 1, 8);
        }

        cv::imwrite("hist.png", ROI);
        cvtColor(ROI, ROI, cv::COLOR_BGR2HSV);
    }else {
        ROI = cv::imread("hist.png", CV_LOAD_IMAGE_COLOR);
        if(!ROI.data) {
            std::cerr <<  "Could not open or find the calibrated hist image!" << std::endl ;
            std::terminate();
        }
        cvtColor(ROI, ROI, cv::COLOR_BGR2HSV);
    }
    cv::MatND hist;

    int hbins = 25, sbins = 25;
    int histSize[] = {hbins, sbins};

    float hranges[] = { 0, 180 };
    float sranges[] = { 0, 256 };

    const float* ranges[] = {hranges, sranges};
    int channels[] = {0, 1};

    calcHist(&ROI, 1, channels, cv::Mat(), hist, 2, histSize, ranges, true, false);
    normalize(hist, hist, 0, 255, NORM_MINMAX, -1, cv::Mat());

#ifdef UI_ON
    cv::imshow("ROI", ROI);
#endif

    return hist;
}

cv::Point pointingLoc(cv::Mat bigMat, cv::MatND hist)
{

    //int hbins = 25, sbins = 25;
    //int histSize[] = {hbins, sbins};

    float hranges[] = { 0, 180 };
    float sranges[] = { 0, 256 };

    const float* ranges[] = {hranges, sranges};

    int channels[] = {0, 1};

    //  int64 e1 = getCPUTickCount();
    //  int64 e2 = getCPUTickCount();
    //  float times = (float)((e2 - e1)/ getTickFrequency());
    //  cout << "TIME: " <<  times << endl;

    cv::Mat HSVMat;
    cvtColor(bigMat, HSVMat, cv::COLOR_BGR2HSV);

    MatND backproj;
    cv::calcBackProject(&HSVMat, 1, channels, hist, backproj, ranges, 1, true);

    cv::Point pointLocation;
    pointLocation.x = bigMat.cols /2;
    pointLocation.y = bigMat.rows /2;

    threshold(backproj,backproj, 0, 255, THRESH_BINARY);

    int morpType = MORPH_ELLIPSE;

    int dilateMorpSize = 4;
    Mat elementDilate = getStructuringElement( morpType, Size( 2*dilateMorpSize + 1, 2*dilateMorpSize+1 ), Point( dilateMorpSize, dilateMorpSize ) );

    int erodeMorpSize = 3;
    Mat elementErode = getStructuringElement( morpType, Size( 2*erodeMorpSize + 1, 2*erodeMorpSize+1 ), Point( erodeMorpSize, erodeMorpSize ) );


    cv::Mat morphMat = backproj.clone();
    cv::GaussianBlur(morphMat, morphMat, Size(3,3), 0);
    cv::dilate(backproj, morphMat, elementDilate );
    cv::erode(morphMat, morphMat, elementErode );

    cv::Mat morphMatProc = morphMat.clone();
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(morphMatProc, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    std::vector<std::vector<cv::Point> > rContours;
    for(unsigned int i=0;i<contours.size();i++)
    {
        if(contourArea(contours.at(i)) >= 3000)
            rContours.push_back(contours.at(i));
    }
    std::vector<std::vector<Point> >hull(rContours.size());
    //vector<vector<int> > hullsI(rContours.size()); // Indices to contour points
    //vector<vector<Vec4i>> defects(rContours.size());

    int largestContourID = 0;
    for(unsigned int i = 0; i < rContours.size(); i++)
    {
        convexHull(rContours.at(i), hull.at(i), false);
        if(contourArea(rContours.at(i)) > contourArea(rContours.at(largestContourID)))
            largestContourID = i;
        //convexHull(rContours[i], hullsI[i], false); 
        //convexityDefects(rContours[i], hullsI[i], defects[i]);
    }

    //TODO:Delete in the final relase!!!
    cv::Mat debugMat = bigMat.clone();
    //cv::Mat hullMat = Mat::zeros(bigMat.rows,bigMat.cols, bigMat.type());
    //cv::Mat contMat = Mat::zeros(bigMat.rows,bigMat.cols, bigMat.type());

    drawContours(debugMat,rContours,-1,cv::Scalar(0,0,255),1);

    //drawContours(hullMat,hull,-1,cv::Scalar(255,0,0),1);
    drawContours(debugMat,hull,-1,cv::Scalar(255,0,0),1); 

    if(hull.size())
    {
        cv::Point lowestConvexY = hull.at(largestContourID).at(0);
        for(int i =0; i < hull.at(largestContourID).size(); i++)
        {
            if(lowestConvexY.y > hull.at(largestContourID).at(i).y)
                lowestConvexY = hull.at(largestContourID).at(i);
        }

        circle(debugMat,lowestConvexY ,2, Scalar(255, 0,0), 5);
        pointLocation = lowestConvexY;
    }


#ifdef UI_ON
    imshow( "Debug", debugMat );
    imshow("Morp", morphMat);
#endif

    return pointLocation;
}

cv::Scalar getHSVAtPoint(cv::Mat bigMat, cv::Point ptToProc)
{
    cv::Mat HSV;
    cv::Mat RGB=bigMat(cv::Rect(ptToProc.x,ptToProc.y,1,1));
    cvtColor(RGB, HSV, cv::COLOR_BGR2HSV);

    Vec3b HSVArray=HSV.at<Vec3b>(0,0);
    int H=HSVArray.val[0];
    int S=HSVArray.val[1];
    int V=HSVArray.val[2];

    return cv::Scalar(H,S,V);
}

