#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "face-capture.h"
#include "errors.h"
#include "capture.h"

typedef struct FaceCapture {
  Core_t core;
} FaceCapture;

void* faceCaptureThread(void *arg) {

  FaceCapture *c = (FaceCapture*)arg;

  Capture_t cap = captureInit();
  CaptureResults results;

  while (1) {
    capture(cap, &results);

    Event e;
    e.whenOccurred = now();
    e.type = E_FACE;
    e.face.numFaces = results.numFaces;
    for (int i=0; i<results.numFaces; i++) {
      e.face.faces[i].x = results.faces[i].x;
      e.face.faces[i].y = results.faces[i].y;
      e.face.faces[i].width = results.faces[i].width;
      e.face.faces[i].height = results.faces[i].height;
    }
    send(c->core, e);
  }

  return NULL;
}

FaceCapture* faceCaptureInit(Core_t core) {
  FaceCapture *c = malloc(sizeof(FaceCapture));
  if (c == NULL) {
    explode("failed to malloc");
  }
  c->core = core;
  return c;
}

void faceCaptureStart(FaceCapture *c) {
  pthread_t threadId;
  pthread_create(&threadId, NULL, faceCaptureThread, c);
}

void faceCaptureStop(FaceCapture *c) {

}
