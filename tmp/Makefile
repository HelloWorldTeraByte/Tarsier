CC = g++
CFLAGS = -std=c++11
ARG = `pkg-config opencv --cflags --libs`

all: 
	$(CC) $(CFLAGS) -g -o Tarsier tarsier.cpp handRecognition.cpp $(ARG)
nahnet: 
	$(CC) $(CFLAGS) -g -o Tarsier reFocus.cpp handRecognition.cpp txtProc.cpp $(ARG)


light:
	$(CC) -I/usr/local/include/opencv -I/usr/local/include/opencv2 -L/usr/local/lib/ -g -o Tarsier tarsier.cpp -lopencv_core -lopencv_imgproc -lopencv_video -lopencv_highgui

