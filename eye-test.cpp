#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include "ps3eye.h"
#include "errors.h"
#include <stdbool.h>
#include <inttypes.h>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

uint64_t now() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t millis = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  return millis;
}

int main() {

  const auto eyeDevices = ps3eye::PS3EYECam::getDevices();
  const auto device = eyeDevices.front();

  int width = 320;
  int height = 240;
  int fps = 187;

  device->init(width, height, fps, ps3eye::PS3EYECam::EOutputFormat::BGR);


//  autogain = false;
//  gain = 20;
//  exposure = 120;
//  sharpness = 0;
//  hue = 143;
//  awb = false;
//  brightness = 20;
//  contrast =  37;
//  blueblc = 128;
//  redblc = 128;
//  greenblc = 128;
//  flip_h = false;
//  flip_v = false;


//  uint8_t gain; // 0 <-> 63
//  uint8_t exposure; // 0 <-> 255
//  uint8_t sharpness; // 0 <-> 63
//  uint8_t hue; // 0 <-> 255
//  bool awb;
//  uint8_t brightness; // 0 <-> 255
//  uint8_t contrast; // 0 <-> 255
//  uint8_t blueblc; // 0 <-> 255
//  uint8_t redblc; // 0 <-> 255
//  uint8_t greenblc; // 0 <-> 255

  device->setAutogain(true);
//  device->setSharpness(63);
//  device->setHue(255);
//  device->setContrast(70);


  device->start();

//  PS3EYE_FORMAT_RGB,          // Output in RGB. Destination buffer must be width * height * 3 bytes
  size_t bufSize = width * height * sizeof(uint8_t) * device->getOutputBytesPerPixel();
  uint8_t *buf = (uint8_t*)malloc(bufSize);
  Mat rgb(height, width, CV_8UC3, buf);

  CascadeClassifier *cascade = new CascadeClassifier();
  cascade->load("/usr/local/Cellar/opencv/4.1.2/share/opencv4/haarcascades/haarcascade_frontalface_default.xml");


  uint64_t last = now();

  while (true) {
    device->getFrame(buf);

    Mat frame1 = rgb.clone();

    Mat gray;
    cvtColor(frame1, gray, COLOR_BGR2GRAY); // Convert to Gray Scale

    vector<Rect> faces;
    cascade->detectMultiScale(frame1, faces, 1.1, 4);

    for (int i = 0; i < std::min((int) faces.size(), 10); i++) {
      Rect f = faces[i];
      rectangle(frame1, Point(f.x, f.y), Point(f.x + f.width, f.y + f.height), (255, 0, 0), 2);
    }

    imshow("Live", frame1);
    waitKey(1);

    uint64_t end = now();
    printf("took: %" PRIu64 "\n", end - last);
    last = end;
  }



}

// // Copy the data into an OpenCV Mat structure
//     cv::Mat mat16uc1_bayer(height, width, CV_16UC1, buf);
//
// // Decode the Bayer data to RGB but keep using 16 bits per channel
//     cv::Mat mat16uc3_rgb(width, height, CV_16UC3);
//     cv::cvtColor(mat16uc1_bayer, mat16uc3_rgb, cv::COLOR_BayerGR2RGB);
//
// // Convert the 16-bit per channel RGB image to 8-bit per channel
//     cv::Mat mat8uc3_rgb(width, height, CV_8UC3);
//     mat16uc3_rgb.convertTo(mat8uc3_rgb, CV_8UC3, 1.0/256); //this could be perhaps done more effectively by cropping bits


//    cvtColor(frame1, gray, COLOR_BGR2GRAY); // Convert to Gray Scale
