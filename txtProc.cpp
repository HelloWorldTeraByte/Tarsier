#include <opencv2/core/core.hpp> 
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>

#include "txtProc.h"

using namespace std;

//Sort the array of rect by their x value
bool sortByX(cv::Rect & a, cv::Rect &b) {
    return a.x < b.x;
}

std::vector<cv::Rect> getWords(cv::Mat inMat)
{
    std::vector<unsigned char> spacePos;
    std::vector<cv::Rect> foundLetters;
    std::vector<cv::Rect> foundWords;
    foundLetters = getLetters(inMat, spacePos);

    //add an space to the end of a word to make processing easier
    if(foundLetters.size())
    {
        cv::Point endLTop, endRBottom;

        //i-1 because vector is 0-index based
        endLTop.x = foundLetters.at(foundLetters.size() -1).br().x;
        endLTop.y = foundLetters.at(foundLetters.size() -1).tl().y + 1;
        endRBottom.x = foundLetters.at(foundLetters.size() -1).br().x + 1;
        endRBottom.y = foundLetters.at(foundLetters.size() -1).br().y;

        cv::Rect endRect(endLTop, endRBottom);

        foundLetters.push_back(endRect);
        spacePos.push_back('1');
    }

    int goBack = 0;
    for(int i = 0; i < foundLetters.size(); i++)
    {
        //cushion each rectangle with extra spacing
        cv::Point tl = foundLetters.at(i).tl();
        cv::Point br = foundLetters.at(i).br();
        tl.y -= 2;
        br.y += 2;
        foundLetters.at(i) = cv::Rect(tl, br);

        if(spacePos.at(i) == '1')
        {
            cv::Point topLeft;
            cv::Point bottomRight;

            int maxHeight = 0;
            for(int j = 0; j < goBack +1; j++)
            {
                if(foundLetters.at(i-j).height > maxHeight)
                    maxHeight = foundLetters.at(i-j).height;
            }

            topLeft.y = foundLetters.at(i-goBack).br().y - maxHeight;
            topLeft.x = foundLetters.at(i-goBack).tl().x;

            bottomRight.x = foundLetters.at(i).br().x - foundLetters.at(i).width;

            int maxY = 0;
            for(int l = 0; l < goBack +1; l++)
            {
                if(foundLetters.at(i-l).br().y >  maxY)
                    maxY = foundLetters.at(i-l).br().y;
            }

            //bottomRight.y = foundLetters.at(i).br().y;
            bottomRight.y = maxY;

            cv::Rect spaceRect(topLeft, bottomRight);
            foundWords.push_back(spaceRect);
            goBack = 0;
        }
        else
            goBack++;

    }

    return foundWords;
}

//Dummy getLetters function without using vector of char for spacePos
std::vector<cv::Rect> getLetters(cv::Mat inMat)
{
    std::vector<unsigned char> spacePos;
    std::vector<cv::Rect> foundLetters;
    foundLetters = getLetters(inMat, spacePos);

    return foundLetters;
}

std::vector<cv::Rect> getLetters(cv::Mat inMat, std::vector<unsigned char> &spacePos)
{
    //TODO: delete in the final release!!!!
    cv::Mat debugMat = inMat.clone();

    std::vector< std::vector< cv::Point> > letterContours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(inMat, letterContours, hierarchy, CV_RETR_EXTERNAL , CV_CHAIN_APPROX_SIMPLE);
    std::vector<std::vector<cv::Point> > contoursPoly( letterContours.size() );
    std::vector<cv::Rect> foundLetters;

    //sort(contoursPoly.begin(),contoursPoly.end(),sortByX());
    for( int i = 0; i< letterContours.size(); i++ ) {
        cv::approxPolyDP( cv::Mat(letterContours.at(i)), contoursPoly.at(i), APPROX_POLY_EP , true );
        cv::Rect appRect(boundingRect(cv::Mat(contoursPoly.at(i))));
        if(appRect.height > LOWEST_LETTER_AREA  && appRect.width > LOWEST_LETTER_AREA)
            //Add found letters to the vector of foundLetters
            foundLetters.push_back(appRect);
    }

    //sort( foundLetters.begin(), foundLetters.end(), sortByX );
    sort( foundLetters.begin(), foundLetters.end(), sortByX);

 
    //Stuff like i and equal sign
    //When two found contours are on top of each other combine them to one element 
    for(int i = 1; i < foundLetters.size(); i++)
    {

        //If the space between the two elements is smaller than xEpsilon
        if(abs(foundLetters.at(i).x - foundLetters.at(i-1).x) < X_EPSILON && 0)
        {  
            cv::Point tLeftPt, bRightPt;

            //Work out the most upper corner
            tLeftPt.x = std::min(foundLetters.at(i).tl().x, foundLetters.at(i-1).tl().x);
            tLeftPt.y = std::min(foundLetters.at(i).tl().y, foundLetters.at(i-1).tl().y);

            //Work out the most lower corner
            bRightPt.x = std::max(foundLetters.at(i).br().x, foundLetters.at(i-1).br().x);
            bRightPt.y = std::max(foundLetters.at(i).br().y, foundLetters.at(i-1).br().y);

            //Remove the i-1 element and change the i elememnt to the new upper rectangle
            foundLetters.at(i) = cv::Rect(tLeftPt, bRightPt);
            foundLetters.erase(foundLetters.begin() + (i-1));

        }

    }
    sort( foundLetters.begin(), foundLetters.end(), sortByX);

    spacePos.resize(foundLetters.size());

    //Find spaces
    for(int i = 1; i < foundLetters.size(); i++)
    {
        //if(abs(centerI.x - centerI1.x) > 8)
        //
        if(abs(foundLetters.at(i).tl().x - foundLetters.at(i-1).br().x) > SPACE_WIDTH)
        {
            cv::Point topLeft;
            cv::Point bottomRight;

            topLeft.x = foundLetters.at(i-1).br().x;
            topLeft.y = foundLetters.at(i-1).br().y - foundLetters.at(i-1).height + 2;
            bottomRight.x = foundLetters.at(i).tl().x;
            ////////////////// should be +2 instead -2
            bottomRight.y = foundLetters.at(i).tl().y + foundLetters.at(i).height - 2;
            cv::Rect spaceRect(topLeft, bottomRight);

            //Add an space in according nth place in the vector
            //Both the foundLetter->Rect and the spacePos->chars
            foundLetters.insert(foundLetters.begin()+i, spaceRect);
            spacePos.insert(spacePos.begin()+i, '1');
            i++;
        }

    }

#ifdef OS_UNIX
    for (int i = 0; i < foundLetters.size(); i++) {

        cv::rectangle(debugMat, foundLetters.at(i), cv::Scalar(255, 0, 0));
        cv::imshow("DebugMat", debugMat);

    }
#endif

    return foundLetters;
}
