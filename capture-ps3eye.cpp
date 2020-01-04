#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "ps3eye.h"

using namespace cv;
using namespace std;

typedef struct Capture {
  ps3eye::PS3EYECam *device;
  CascadeClassifier *cascade;
  uint8_t *buf;
  Mat *bgr;
} Capture;

#define FPS 187

extern "C" {
#include "capture.h"

Capture* captureInit() {

  Capture *c = (Capture *) malloc(sizeof(Capture));
  if (c == NULL) {
    printf("malloc failed");
    exit(-1);
  }

  const auto eyeDevices = ps3eye::PS3EYECam::getDevices();

  c->device = eyeDevices.front().get();
  c->device->init(CAPTURE_WIDTH, CAPTURE_HEIGHT, FPS, ps3eye::PS3EYECam::EOutputFormat::BGR);
  c->device->setAutogain(true);
  c->device->start();

  // BGR: destination buffer must be width * height bytes * 3
  size_t bufSize = CAPTURE_WIDTH * CAPTURE_HEIGHT * sizeof(uint8_t) * c->device->getOutputBytesPerPixel();
  c->buf = (uint8_t*)malloc(bufSize);
  c->bgr = new Mat(CAPTURE_HEIGHT, CAPTURE_WIDTH, CV_8UC3, c->buf);

  CascadeClassifier *cascade = new CascadeClassifier();
  cascade->load("/usr/local/Cellar/opencv/4.1.2/share/opencv4/haarcascades/haarcascade_frontalface_default.xml");
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
//  uint64_t start = now1();

  c->device->getFrame(c->buf);

  Mat gray;
  cvtColor(*c->bgr, gray, COLOR_BGR2GRAY); // Convert to Gray Scale

  vector<Rect> faces;
  c->cascade->detectMultiScale(gray, faces, 1.2, 3);

  for (int i = 0; i < std::min((int) faces.size(), 10); i++) {
    Rect f = faces[i];
    rectangle(*c->bgr, Point(f.x, f.y), Point(f.x + f.width, f.y + f.height), (255, 0, 0), 2);
  }

  int centerX = CAPTURE_WIDTH / 2;
  int centerY = CAPTURE_HEIGHT / 2;
  circle(*c->bgr, Point(centerX, centerY), CAPTURE_CIRCLE_RADIUS, (255, 0, 0), 2);

  results->numFaces = faces.size();
  for (int i = 0; i < faces.size(); i++) {
    results->faces[i].x = faces.at(i).x;
    results->faces[i].y = faces.at(i).y;
    results->faces[i].width = faces.at(i).width;
    results->faces[i].height = faces.at(i).height;
  }

//  imshow("Live", *c->bgr);
//  waitKey(1);

//  uint64_t end = now1();
//  printf("capture and recognize: %" PRIu64 "ms\n", end - start);
}

void captureCleanup(Capture *c) {
}

}
