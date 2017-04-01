#include <stdio.h>
#include <signal.h>


#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

static volatile int keepRunning = 1;
void intHandler(int dummy)
{
    keepRunning = 0;
}

int main(int argc, char **argv) 
{

    cv::VideoCapture cap(0); 
    setVideoSettings(cap);

    if(!cap.isOpened()) {
        perror("ERROR: Cannot open the Video!");
        return -1;
    }

    signal(SIGINT, intHandler);

    cv::Mat frame;
    cv::MatND hist;

    hist = calibrateToColor(bigMat, bCalibrate);

    while(keepRunning) {

        bool bSuccess = cap.read(frame);
        if (!bSuccess) {
            std::cerr << "ERROR: Cannot read from the video!" << std::endl;
            return -1;
        }
    }

    printf(" Exited succesfully\n");

    return 0;
}
