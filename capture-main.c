#include "capture.h"
#include <unistd.h>

int main() {

  Capture_t cap = captureInit();
  CaptureResults results;

  while (1) {
    capture(cap, &results);
  }
}

