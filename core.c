#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <math.h>
#include <sys/time.h>
#include <errno.h>

#include "errors.h"
#include "core.h"
#include "controller.h"
#include "launcher.h"
#include "face-capture.h"
#include "capture.h"

void msleep(uint64_t ms) {
  usleep(ms * 1000);
}

uint64_t now() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t millis = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  return millis;
}

typedef enum {
  LED_OFF,
  LED_ON,
  LED_BLINK,
} LedMode;

typedef enum {
  SENTRY_MODE_PASSIVE,
  SENTRY_MODE_ARMED,
} SentryMode;

typedef enum {
  MODE_MANUAL,
  MODE_SENTRY,
} ControlMode;

typedef struct Core {
  pthread_mutex_t mutex;
  pthread_cond_t fireCond;

  bool continueFiring; // set by controller inputs, read by fire thread

  Launcher_t launcher;

  ControlMode mode;
  uint8_t remainingShots;
  LedMode ledMode;
  bool ledOn;

  SentryMode sentryMode;
  bool trackingFace;
  bool moving;

} Core;

#define FIRING_MAX_CAPACITY 4

void coreInit(Core *c) {
  pthread_mutex_init(&c->mutex, NULL);
  pthread_cond_init(&c->fireCond, NULL);
  c->mode = MODE_MANUAL;
  c->remainingShots = FIRING_MAX_CAPACITY;
  c->ledMode = LED_OFF;
  c->sentryMode = SENTRY_MODE_PASSIVE;
  c->launcher = NULL;
  c->ledOn = false;
  c->trackingFace = false;
  c->moving = false;
}

void toggleMode(Core *core) {
  // TODO
}

void reloaded(Core *core) {
  printf("info: reloading\n");
  core->remainingShots = FIRING_MAX_CAPACITY;
}

#define LED_BLINK_DURATION 100

void* ledTimerFn(void *arg) {
  Core *core = (Core*)arg;

  while (true) {
    msleep(LED_BLINK_DURATION);

    pthread_mutex_lock(&core->mutex);
    switch (core->ledMode) {
      case LED_OFF:
      case LED_ON:
        break;
      case LED_BLINK: {
        LauncherCmd cmd;
        if (core->ledOn) {
          core->ledOn = false;
          cmd = LAUNCHER_LEDOFF;
        } else {
          core->ledOn = true;
          cmd = LAUNCHER_LEDON;
        }
        launcherSend(core->launcher, cmd);
        break;
      }
      default:
        explode("unknown ledmode: %u\n", core->ledMode);
    }
    pthread_mutex_unlock(&core->mutex);
  }

  return NULL;
}

void ledTimerStart(Core *core) {
  pthread_t threadId;
  pthread_create(&threadId, NULL, ledTimerFn, core);
}

void toggleLed(Core *core) {

  if (core->mode != MODE_MANUAL) {
    printf("info: ignoring led toggle in non-manual mode\n");
    return;
  }

  switch (core->ledMode) {
    case LED_OFF:
      core->ledMode = LED_ON;
      launcherSend(core->launcher, LAUNCHER_LEDON);
      break;
    case LED_ON:
      core->ledMode = LED_BLINK; // scheduler will pick this up
      break;
    case LED_BLINK:
      core->ledMode = LED_OFF;
      launcherSend(core->launcher, LAUNCHER_LEDOFF);
      break;
    default:
      explode("unknown led mode");
  }
}

#define FIRE_DURATION 3300

void* firingTimerFn(void *arg) {
  Core *core = (Core*)arg;

  // just used for sleeping
  pthread_cond_t sleepCond;
  pthread_cond_init(&sleepCond, NULL);

  pthread_mutex_lock(&core->mutex);

  while (true) {

    while (core->continueFiring && core->remainingShots > 0) {
      printf("firing (remainingShots = %u)\n", core->remainingShots);
      launcherSend(core->launcher, LAUNCHER_FIRE);

      pthread_mutex_unlock(&core->mutex);
      msleep(FIRE_DURATION);
      pthread_mutex_lock(&core->mutex);

      core->remainingShots -= 1;
    }

    if (core->continueFiring && core->remainingShots == 0) {
      printf("out of ammo!\n");
    }

    printf("waiting for signal to start firing\n");
    int rt = pthread_cond_wait(&core->fireCond, &core->mutex);
    switch (rt) {
      case 0:
        break;
      case EINVAL:
        explode("something about the wait is invalid");
    }
  }

  pthread_mutex_unlock(&core->mutex);
  return NULL;
}

void firingTimerStart(Core *core) {
  pthread_t threadId;
  pthread_create(&threadId, NULL, firingTimerFn, core);
}

void beginFiring(Core *core) {

  if (core->mode != MODE_MANUAL) {
    printf("info: ignoring begin-firing non-manual mode\n");
    return;
  }

  core->continueFiring = true;

  int rt = pthread_cond_signal(&core->fireCond);
  switch (rt) {
    case 0:
      break;
    case EINVAL: explode("something about the signal is invalid");
  }
}

void endFiring(Core *core) {
  if (core->mode != MODE_MANUAL) {
    printf("info: ignoring end-firing non-manual mode\n");
    return;
  }
  core->continueFiring = false;
}

void move(Core *core, Movement m) {

  if (core->mode != MODE_MANUAL) {
    printf("info: ignoring move in non-manual mode\n");
    return;
  }

  if (core->continueFiring) {
    printf("info: ignoring move while firing\n");
    return;
  }

  LauncherCmd cmd;
  switch (m) {
    case MOVE_UP:
      cmd = LAUNCHER_UP;
      break;
    case MOVE_DOWN:
      cmd = LAUNCHER_DOWN;
      break;
    case MOVE_LEFT:
      cmd = LAUNCHER_LEFT;
      break;
    case MOVE_RIGHT:
      cmd = LAUNCHER_RIGHT;
      break;
    case MOVE_NONE:
      cmd  = LAUNCHER_STOP;
      break;
    default:
      explode("unknown movement: %u\n", m)
  }
  launcherSend(core->launcher, cmd);
}

void toggleSentryMode(Core *core) {

  if (core->mode != MODE_SENTRY) {
    printf("info: ignoring toggle-sentry-mode in manual mode\n");
    return;
  }
}

void handleControl(Core *core, ControlEvent e) {

  switch (e.type) {
    case CONTROL_TYPE_MODE_TOGGLE:
      toggleMode(core);
      break;
    case CONTROL_TYPE_RELOAD:
      reloaded(core);
      break;

    // manual controls
    case CONTROL_TYPE_LED_TOGGLE:
      toggleLed(core);
      break;
    case CONTROL_TYPE_FIRE_BEGIN:
      beginFiring(core);
      break;
    case CONTROL_TYPE_FIRE_END:
      endFiring(core);
      break;
    case CONTROL_TYPE_MOVEMENT:
      move(core, e.movement);
      break;

    // sentry controls
    case CONTROL_TYPE_SENTRY_MODE_TOGGLE: // passive, armed
      toggleSentryMode(core);
      break;
  }
}

bool isInside(int circle_x, int circle_y, int rad, int x, int y) {
  if ((x - circle_x) * (x - circle_x) +
      (y - circle_y) * (y - circle_y) <= rad * rad)
    return true;
  else
    return false;
}

void handleFace(Core *core, FaceEvent e) {

  if (e.numFaces == 0) {
    if (core->trackingFace) {
      // nothing to track now
      launcherSend(core->launcher, LAUNCHER_STOP);
      core->trackingFace = false;
      core->moving = false;
    }
  }
  else {

    CapturedFace *largestFace = NULL;
    for (int i=0; i<e.numFaces; i++) {
      if (largestFace == NULL || e.faces[i].width > largestFace->width) {
        largestFace = &e.faces[i];
      }
    }
    if (largestFace == NULL) {
      explode("didn't get a face");
    }

    int centerX = CAPTURE_WIDTH / 2;
    int centerY = CAPTURE_HEIGHT / 2;

    int faceCenterX = largestFace->x + (largestFace->width / 2);
    int faceCenterY = largestFace->y + (largestFace->height / 2);

    // determine face center's distance

    /*
     * idea:
     * there could be near, middle, and far range circles for determining centeredness
     * which circle applies can be based on how big the face is
     *
     * need to determine distance based on face size
     * if you're too far away, don't even attempt tracking
     * closer, you might be close enough to attempt tracking, but not firing because too inaccurate
     * - in this mode we should have a high threshold for actually moving the launcher, fuzzy idea of 'centered'
     * then, there are roughly two accurate firing ranges
     * - your face is close enough that it can be reasonably targeted at all (face fits in target circle)
     * - .. there's a squishy window in the middle here we have to figure out
     * - your face is close enough that it can be directly centered (target circle fits in face)
     *
     * another idea, when within a certain close angular distance, only attempt to fine-adjust 3 times
     *
     * another idea: sounds would be fun: https://www.zedge.net/find/ringtones/oblivion
     */

    if (isInside(centerX, centerY, 50, faceCenterX, faceCenterY)) {
      printf("face centered on cirlce\n");
      if (core->moving) {
        launcherSend(core->launcher, LAUNCHER_STOP);
      }
    }
    else if (   (centerX > largestFace->x)
        && (centerX < (largestFace->x + largestFace->width))
        && (centerY > largestFace->y)
        && (centerY < (largestFace->y + largestFace->height))) {
      printf("center of screen inside face box\n");
      if (core->moving) {
        launcherSend(core->launcher, LAUNCHER_STOP);
      }
    }
    else {

      int xAbs = abs(centerX - faceCenterX);
      int yAbs = abs(centerY - faceCenterY);

      if (xAbs >= yAbs) {
        // move horizontally
        if (faceCenterX < centerX) {
          // move left
          launcherSend(core->launcher, LAUNCHER_LEFT);
        }
        else {
          // move right
          launcherSend(core->launcher, LAUNCHER_RIGHT);
        }
      }
      else {
        // move vertically
        if (faceCenterY < centerY) {
          // move up
          launcherSend(core->launcher, LAUNCHER_UP);
        }
        else {
          // move down
          launcherSend(core->launcher, LAUNCHER_DOWN);
        }
      }
    }

    core->trackingFace = true;
  }



  // TODO: write the sentry code

  // TODO: figure out how to continue displaying images with imshow (only works on the main thread)
  // could just use the main thread (which isn't doing anything) to drive the capture logic
}

char* controlTypeName(ControlType t) {
  switch (t) {
    case CONTROL_TYPE_MODE_TOGGLE:
      return "mode-toggle";
    case CONTROL_TYPE_RELOAD:
      return "reload";
    case CONTROL_TYPE_LED_TOGGLE:
      return "led-toggle";
    case CONTROL_TYPE_FIRE_BEGIN:
      return "fire-begin";
    case CONTROL_TYPE_FIRE_END:
      return "fire-end";
    case CONTROL_TYPE_MOVEMENT:
      return "move";
    case CONTROL_TYPE_SENTRY_MODE_TOGGLE:
      return "sentry-mode-toggle";
    default:
      explode("unknown control type: %u\n", t);
  }
}

char* movementName(Movement m) {
  switch (m) {
    case MOVE_UP:
      return "up";
    case MOVE_DOWN:
      return "down";
    case MOVE_LEFT:
      return "left";
    case MOVE_RIGHT:
      return "right";
    case MOVE_NONE:
      return "none";
    default:
    explode("unknown movement: %u\n", m);
  }
}

void printEvent(Event e) {
  switch (e.type) {
    case E_CONTROL:
      if (e.control.type == CONTROL_TYPE_MOVEMENT) {
        printf("{\"type\": \"control\",  \"whenOccurred\": \"%" PRIu64 "\", \"control-type\": \"%s\", \"direction\": \"%s\"}\n",
            e.whenOccurred, controlTypeName(e.control.type), movementName(e.control.movement));
      }
      else {
        printf("{\"type\": \"control\",  \"whenOccurred\": \"%" PRIu64 "\", \"control-type\": \"%s\"}\n", e.whenOccurred,
               controlTypeName(e.control.type));
      }
      break;
    case E_FACE:
      printf("{\"type\": \"face\",  \"whenOccurred\": \"%" PRIu64 "\"}\n", e.whenOccurred);
      break;
    default:
    explode("unknown event type: %u\n", e.type);
  }
  fflush(stdout);
}

bool send(Core *core, Event e) {
  pthread_mutex_lock(&core->mutex);

  switch (e.type) {
    case E_CONTROL:
      printEvent(e);
      handleControl(core, e.control);
      break;
    case E_FACE:
      handleFace(core, e.face);
      break;
    default:
      printf("error: unknown event type encountered, %u\n", e.type);
      break;
  }

  pthread_mutex_unlock(&core->mutex);
  return true;
}

int main() {

  Core core;
  coreInit(&core);

  Launcher_t launcher = launcherStart();
  Controller_t controller = controllerInit(&core);
//  FaceCapture_t capture = faceCaptureInit(&core);

  core.launcher = launcher;

  controllerStart(controller);
  ledTimerStart(&core);
  firingTimerStart(&core);

  Capture_t cap = captureInit();
  CaptureResults results;

//  uint64_t last = now();

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
    send(&core, e);

//    uint64_t this = now();
//    printf("cycle: %" PRIu64 "\n", this - last);
//    last = this;
  }


//  faceCaptureStart(capture);
//
//  while (true) {
//    sleep(10);
//  }
}

