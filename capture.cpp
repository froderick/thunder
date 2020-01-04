#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

using namespace cv;
using namespace std;

typedef struct Capture {
  VideoCapture *cap;
  CascadeClassifier *cascade;
} Capture;

extern "C" {
#include "capture.h"

Capture* captureInit() {

  int deviceID = 1;             // 0 = open default camera
  int apiID = cv::CAP_ANY;      // 0 = autodetect default API

  VideoCapture *cap = new VideoCapture();

  cap->open(deviceID + apiID);
  if (!cap->isOpened()) {
    printf("ERROR! Unable to open camera\n");
    exit(-1);
  }

  cap->set(CAP_PROP_FRAME_WIDTH, CAPTURE_WIDTH);
  cap->set(CAP_PROP_FRAME_HEIGHT, CAPTURE_HEIGHT);
  cap->set(CAP_PROP_FPS, 60);

  CascadeClassifier *cascade = new CascadeClassifier();
  cascade->load("/usr/local/Cellar/opencv/4.1.2/share/opencv4/haarcascades/haarcascade_frontalface_default.xml");

  Capture *c = (Capture *) malloc(sizeof(Capture));
  if (c == NULL) {
    printf("malloc failed");
    exit(-1);
  }

  c->cap = cap;
  c->cascade = cascade;

  return c;
}

uint64_t now1() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t millis = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  return millis;
}

void capture(Capture *c, CaptureResults *results) {


  Mat frame;

  {
    uint64_t start = now1();
    c->cap->read(frame);
    uint64_t end = now1();
    printf("capture: %" PRIu64 "\n", end - start);
  }

  if (frame.empty()) {
    printf("ERROR! blank frame grabbed\n");
    exit(-1);
  }


  uint64_t start = now1();

  Mat frame1 = frame.clone();

  Mat gray;
  vector<Rect> faces;

  cvtColor(frame1, gray, COLOR_BGR2GRAY); // Convert to Gray Scale
  c->cascade->detectMultiScale(gray, faces, 1.1, 4);

  for (int i = 0; i < std::min((int) faces.size(), 10); i++) {
    Rect f = faces[i];
    rectangle(frame1, Point(f.x, f.y), Point(f.x + f.width, f.y + f.height), (255, 0, 0), 2);
  }

  int centerX = CAPTURE_WIDTH / 2;
  int centerY = CAPTURE_HEIGHT / 2;
  circle(frame1, Point(centerX, centerY), 100, (255, 0, 0), 2);

  results->numFaces = faces.size();
  for (int i = 0; i < faces.size(); i++) {
    results->faces[i].x = faces.at(i).x;
    results->faces[i].y = faces.at(i).y;
    results->faces[i].width = faces.at(i).width;
    results->faces[i].height = faces.at(i).height;
  }

  imshow("Live", frame1);
  waitKey(1);

  uint64_t end = now1();
  printf("recognize: %" PRIu64 "\n", end - start);

}

void captureCleanup(Capture *c) {
}

}
