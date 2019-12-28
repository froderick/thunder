#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <stdio.h>

using namespace cv;
using namespace std;

int main(int, char**) {

  int deviceID = 1;             // 0 = open default camera
  int apiID = cv::CAP_ANY;      // 0 = autodetect default API

  Mat frame;
  VideoCapture cap;

  cap.open(deviceID + apiID);
  if (!cap.isOpened()) {
    cerr << "ERROR! Unable to open camera\n";
    return -1;
  }

  cap.set(CAP_PROP_FRAME_WIDTH,1920);
  cap.set(CAP_PROP_FRAME_HEIGHT,1080);

  CascadeClassifier cascade;
//  cascade.load("haarcascade_frontalface_default.xml");
  cascade.load("/usr/local/Cellar/opencv/4.1.2/share/opencv4/haarcascades/haarcascade_frontalface_default.xml");

  cout << "Start grabbing" << endl
       << "Press any key to terminate" << endl;

  for (;;) {
    cap.read(frame);
    if (frame.empty()) {
      cerr << "ERROR! blank frame grabbed\n";
      break;
    }

    Mat frame1 = frame.clone();
    Mat gray;
    vector<Rect> faces, faces2;

    cvtColor(frame1, gray, COLOR_BGR2GRAY); // Convert to Gray Scale
    cascade.detectMultiScale(gray, faces, 1.1, 4);

//    cascade.detectMultiScale(frame1, faces, 1.1,
//                              2, 0|CASCADE_SCALE_IMAGE, Size(30, 30) );

    // Draw circles around the faces
    for (size_t i = 0; i < faces.size(); i++) {
      Rect f = faces[i];
      rectangle(frame1, Point(f.x, f.y), Point(f.x+f.width, f.y+f.height), (255, 0, 0), 2);
    }

    imshow("Live", frame1);

    cv::imwrite("test.jpg", frame1);

    if (waitKey(4) >= 0)
      break;

  }

  return 0;
}

/*
 * next steps
 * - may as well just convert this to c?
 * - set up a streaming post with curl, sending each captured image to the api
 */

