#ifndef THUNDER_CORE_H
#define THUNDER_CORE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * time utils
 */

uint64_t now();

/*
 * control event
 */

typedef enum {
  MOVE_UP,
  MOVE_DOWN,
  MOVE_LEFT,
  MOVE_RIGHT,
  MOVE_NONE,
} Movement;

typedef enum {
  CONTROL_TYPE_MODE_TOGGLE,
  CONTROL_TYPE_RELOAD,
  // manual controls
  CONTROL_TYPE_LED_TOGGLE,
  CONTROL_TYPE_FIRE_BEGIN,
  CONTROL_TYPE_FIRE_END,
  CONTROL_TYPE_MOVEMENT,
  // sentry controls
  CONTROL_TYPE_SENTRY_MODE_TOGGLE, // passive, armed
} ControlType;

typedef struct {
  ControlType type;
  union {
    Movement movement;
  };
} ControlEvent;

/*
 * face event
 */

typedef struct CapturedFace {
  int x, y, width, height;
} CapturedFace;

#define CAPTURE_MAX_FACES 10

typedef struct {
  uint16_t numFaces;
  CapturedFace faces[CAPTURE_MAX_FACES];
} FaceEvent;

/*
 * sending events
 */

typedef enum {
  E_CONTROL,
  E_FACE,
} EventType;

typedef struct {
  EventType type;
  uint64_t whenOccurred;
  union {
    ControlEvent control;
    FaceEvent face;
  };
} Event;

typedef struct Core* Core_t;

bool send(Core_t core, Event e);

#endif //THUNDER_CORE_H
