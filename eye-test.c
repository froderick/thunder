#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include "ps3eye_capi.h"
#include "errors.h"
#include <stdbool.h>

uint64_t now() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t millis = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  return millis;
}

int main() {

  ps3eye_init();

  printf("num connected: %i\n", ps3eye_count_connected());

  int width = 320;
  int height = 240;

  ps3eye_t *cam = NULL;
  {
    int id = 0;
    int fps = 187;
    ps3eye_format fmt = PS3EYE_FORMAT_RGB;
    cam = ps3eye_open(id, width, height, fps, fmt);
    if (cam == NULL) {
      explode("could not open camera");
    }
  }



//  PS3EYE_FORMAT_RGB,          // Output in RGB. Destination buffer must be width * height * 3 bytes
  size_t bufSize = width * height * sizeof(uint8_t) * 3;
  uint8_t *buf = malloc(bufSize);
  if (buf == NULL) {
    explode("cannot allocate buffer");
  }

  uint64_t last = now();

  while (true) {
    ps3eye_grab_frame(cam, buf);
  }


}