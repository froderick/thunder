#include <stdint.h>

typedef struct Capture* Capture_t;

typedef struct CaptureFace {
  int x, y, width, height;
} CaptureFace;

#define CAPTURE_MAX_FACES 10

#define CAPTURE_CIRCLE_RADIUS 30

#define CAPTURE_WIDTH 320
#define CAPTURE_HEIGHT 240

typedef struct CaptureResults {
  uint16_t numFaces;
  CaptureFace faces[CAPTURE_MAX_FACES];
} CaptureResults;

Capture_t captureInit();
void capture(Capture_t c, CaptureResults *results);
void captureCleanup(Capture_t c);
