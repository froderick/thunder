#ifndef THUNDER_FACE_CAPTURE_H
#define THUNDER_FACE_CAPTURE_H

#include "core.h"

typedef struct FaceCapture* FaceCapture_t;

FaceCapture_t faceCaptureInit(Core_t core);
void faceCaptureStart(FaceCapture_t c);
void faceCaptureStop(FaceCapture_t c);

#endif //THUNDER_FACE_H
