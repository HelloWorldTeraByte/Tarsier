#include <iostream>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#define PACK_SIZE 4096
#define SERVER_PORT 54321
#define SERVER_IP "192.168.1.2"

static volatile int keepRunning = 1;
bool bStartBtnPressed = false;

void diep(char *s)
{
    perror(s);
    exit(1);
}

void setVideoSettings(cv::VideoCapture &cap)
{
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 720);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 480);
}

void intHandler(int dummy) 
{
    keepRunning = 0;
}

int setupTransmit(struct sockaddr_in &serverAddr, int &fd)
{
    if ( (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("Cannot create the socket");
        return -1;
    }

    memset( &serverAddr, 0, sizeof(serverAddr) );
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);              
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    return 0;
}

std::vector<uchar> sliceVec(const std::vector<uchar>& v, int start=0, int end=-1)
{
    int oldlen = v.size();
    int newlen;

    if (end == -1 or end >= oldlen) {
        newlen = oldlen-start;
    } else {
        newlen = end-start;
    }

    std::vector<uchar> nv(newlen);

    for (int i=0; i<newlen; i++) {
        nv[i] = v[start+i];
    }
    return nv;
}

int sendImage(cv::Mat frame, int &fd, struct sockaddr_in &serverAddr)
{
    //TODO: Split image into chuncks before sending
    std::vector<uchar> imgData;
    std::vector<int> compressOpt;
    compressOpt.push_back(CV_IMWRITE_JPEG_QUALITY);
    compressOpt.push_back(80);

    cv::imencode(".jpg", frame, imgData, compressOpt); 
    int totalPacks = 1 + (imgData.size() - 1) / PACK_SIZE;
    uint32_t nmbPacks = htonl(totalPacks);

    if(sendto(fd, &nmbPacks ,sizeof(uint32_t), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror( "Failed to send packets amount!" );
        return -1;
    }

    int lastPt = 0;
    for (int i = 0; i < totalPacks; i++) {
        usleep(2000);

        std::vector<uchar> vecToSend;
        lastPt = i*PACK_SIZE;
        vecToSend = sliceVec(imgData, lastPt, lastPt+PACK_SIZE);

        std::string imgPack(vecToSend.begin(), vecToSend.end());

        if (sendto(fd, imgPack.c_str(), imgPack.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            perror( "Failed to send image!" );
            return -1;
        }

    }

    //if(sendto(fd, &dataLength ,sizeof(uint32_t), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
    //   perror( "Failed to send image size!" );
    //  return -1;
    //}
    //if(sendto(fd, imgPack.c_str(), imgPack.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
    //   perror( "Failed to send the image!" );
    //  return -1;
    //}

    return 0;
}

int main(int argc, char **argv)
{

    cv::VideoCapture cap(0); 
    setVideoSettings(cap);

    if (!cap.isOpened()) {
        perror("ERROR: Cannot open the Video!");
        return -1;
    }

    signal(SIGINT, intHandler);

    cv::Mat frame;
    int fd;
    struct sockaddr_in serverAddr;

    if (setupTransmit(serverAddr, fd) < 0)
        return -1;

    //Pressed button to start

    while (keepRunning) {

        bool bSuccess = cap.read(frame);
        if (!bSuccess) {
            std::cerr << "ERROR: Cannot read from the video!" << std::endl;
            return -1;
        }

        bool bZoom = false;
        if (bZoom) {
            cv::Point tlPt, brPt;
            tlPt.x = frame.cols /2 - 360;
            tlPt.y = frame.rows /2 - 240;

            brPt.x = frame.cols /2 + 360;
            brPt.y = frame.rows /2 + 240;
            cv::Mat sendMat = frame(cv::Rect(tlPt, brPt));


            sendImage(sendMat, fd, serverAddr);
        } else {
            sendImage(frame, fd, serverAddr);
        }

        printf( ".");
        fflush(stdout);

        usleep(300000);
    }

    close(fd);
    printf( " Exited succesfully\n" );

    return 0;
}
