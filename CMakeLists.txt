cmake_minimum_required(VERSION 2.8)
project( ReFocus )
find_package( OpenCV REQUIRED )
find_package( Threads REQUIRED )
add_executable( ReFocus reFocus.cpp handRecognition.cpp txtProc.cpp )
target_link_libraries( ReFocus ${OpenCV_LIBS} lept tesseract ${CMAKE_THREAD_LIBS_INIT}) 
