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
#include "sound.h"

#define SOUND_SENTRY_OFF         "sound/sentry-off.mp3"
#define SOUND_SENTRY_PASSIVE     "sound/sentry-passive.mp3"
#define SOUND_SENTRY_ARMED       "sound/sentry-armed.mp3"
#define SOUND_RELOAD             "sound/reload.mp3"
#define SOUND_NO_AMMO            "sound/no-ammo.mp3"

#define SOUND_TRACKING           "sound/sentry_tracking.mp3"
#define SOUND_SENTRY_DEACTIVATED "sound/sentry_deactivated.mp3"
#define SOUND_6                  "sound/drone.mp3"
#define SOUND_7                  "sound/drone_2.mp3"
#define SOUND_8                  "sound/drone_3.mp3"

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
  LED_BLINK_SLOW,
  LED_BLINK_FAST,
} LedMode;

typedef enum {
  SENTRY_MODE_OFF,
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

  Launcher_t launcher;
  Movement movement;
  bool continueFiring; // set by controller inputs, read by fire thread
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
  c->movement = MOVE_NONE;
  c->continueFiring = false;
  c->remainingShots = FIRING_MAX_CAPACITY;
  c->ledMode = LED_OFF;
  c->sentryMode = SENTRY_MODE_OFF;
  c->launcher = NULL;
  c->ledOn = false;
  c->trackingFace = false;
  c->moving = false;
}

#define LED_BLINK_SLOW_DURATION 500
#define LED_BLINK_FAST_DURATION 100

void* ledTimerFn(void *arg) {
  Core *core = (Core*)arg;

  while (true) {

    switch (core->ledMode) {
      case LED_BLINK_SLOW:
        msleep(LED_BLINK_SLOW_DURATION);
        break;
      case LED_BLINK_FAST:
        msleep(LED_BLINK_FAST_DURATION);
        break;
      default:
        msleep(LED_BLINK_SLOW_DURATION);
    }

      pthread_mutex_lock(&core->mutex);
    switch (core->ledMode) {
      case LED_OFF:
      case LED_ON:
        break;
      case LED_BLINK_SLOW:
      case LED_BLINK_FAST: {
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

void setLedMode(Core *core, LedMode mode) {
  core->ledMode = mode;
  switch (core->ledMode) {
    case LED_OFF:
      launcherSend(core->launcher, LAUNCHER_LEDOFF);
      break;
    case LED_ON:
      launcherSend(core->launcher, LAUNCHER_LEDON);
      break;
    case LED_BLINK_SLOW:
    case LED_BLINK_FAST:
      // timer will pick this up
      break;
    default: explode("unknown led mode");
  }
}

void toggleLed(Core *core) {
  switch (core->ledMode) {
    case LED_OFF:
      setLedMode(core, LED_ON);
      break;
    case LED_ON:
      setLedMode(core, LED_BLINK_SLOW);
      break;
    case LED_BLINK_SLOW:
      setLedMode(core, LED_BLINK_FAST);
      break;
    case LED_BLINK_FAST:
      setLedMode(core, LED_OFF);
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

    if (core->continueFiring) {

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
        playSound(SOUND_NO_AMMO);
      }
    }

    LauncherCmd cmd;
    switch (core->movement) {
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
      explode("unknown movement: %u\n", core->movement)
    }
    launcherSend(core->launcher, cmd);

    printf("waiting for signal to fire or move\n");
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
  core->continueFiring = true;
  int rt = pthread_cond_signal(&core->fireCond);
  switch (rt) {
    case 0:
      break;
    case EINVAL: explode("something about the signal is invalid");
  }
}

void endFiring(Core *core) {
  core->continueFiring = false;
}

void move(Core *core, Movement m) {
  core->movement = m;
  int rt = pthread_cond_signal(&core->fireCond);
  switch (rt) {
    case 0:
      break;
    case EINVAL: explode("something about the signal is invalid");
  }
}

void reload(Core *core) {
  printf("info: reloading\n");
  core->remainingShots = FIRING_MAX_CAPACITY;
  playSound(SOUND_RELOAD);
  int rt = pthread_cond_signal(&core->fireCond);
  switch (rt) {
    case 0:
      break;
    case EINVAL: explode("something about the signal is invalid");
  }
}

void sentryModeChanged(Core *core) {
  switch (core->sentryMode) {
    case SENTRY_MODE_OFF:
      printf("sentry: disabled\n");
      playSound(SOUND_SENTRY_OFF);
      setLedMode(core, LED_OFF);
      break;
    case SENTRY_MODE_PASSIVE:
      printf("sentry: passive mode enabled\n");
      playSound(SOUND_SENTRY_PASSIVE);
      break;
    case SENTRY_MODE_ARMED:
      printf("sentry: armed mode enabled\n");
      playSound(SOUND_SENTRY_ARMED);
      break;
  }
}

void toggleMode(Core *core) {
  core->trackingFace = false;
  core->moving = false;

  if (core->sentryMode == SENTRY_MODE_OFF) {
    core->sentryMode = SENTRY_MODE_PASSIVE;
    sentryModeChanged(core);
  }
  else {
    core->sentryMode = SENTRY_MODE_OFF;
    sentryModeChanged(core);
  }
}

void toggleSentryMode(Core *core) {

  bool changed = false;
  switch (core->sentryMode) {
    case SENTRY_MODE_OFF:
      printf("info: ignoring toggle-sentry-mode when sentry is disabled\n");
      break;
    case SENTRY_MODE_PASSIVE:
      core->sentryMode = SENTRY_MODE_ARMED; // red light is off by default, starts blinking when tracking
      changed = true;
      break;
    case SENTRY_MODE_ARMED:
      core->sentryMode = SENTRY_MODE_PASSIVE; // red light is on by default
      changed = true;
      break;
  }

  if (changed) {
    sentryModeChanged(core);
  }
}

void handleControl(Core *core, ControlEvent e) {

  switch (e.type) {
    case CONTROL_TYPE_MODE_TOGGLE:
      toggleMode(core);
      break;
    case CONTROL_TYPE_RELOAD:
      reload(core);
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

/*
 * light is off when sentry is off
 * blink slowly when sentry is enabled
 * blink faster when sentry is tracking a face
 * solid red when sentry has a face in its sights
 */

void handleFace(Core *core, FaceEvent e) {

  if (core->sentryMode == SENTRY_MODE_OFF) {
    return;
  }

  if (e.numFaces == 0) {
    setLedMode(core, LED_BLINK_SLOW);
    if (core->trackingFace) {
      // nothing to track now
      move(core, MOVE_NONE);
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

    if (isInside(centerX, centerY, CAPTURE_CIRCLE_RADIUS, faceCenterX, faceCenterY)) {
      printf("sentry: face centered on circle\n");
      setLedMode(core, LED_ON);
      if (core->moving) {
        move(core, MOVE_NONE);
        core->moving = false;
        if (core->sentryMode == SENTRY_MODE_ARMED) {
          beginFiring(core);
        }
      }
    }
    else if (
           (centerX > largestFace->x)
        && (centerX < (largestFace->x + largestFace->width))
        && (centerY > largestFace->y)
        && (centerY < (largestFace->y + largestFace->height))) {
      printf("sentry: center of screen inside face box\n");
      setLedMode(core, LED_ON);
      if (core->moving) {
        move(core, MOVE_NONE);
        core->moving = false;
        if (core->sentryMode == SENTRY_MODE_ARMED) {
          printf("sentry: firing\n");
          beginFiring(core);
        }
      }
    }
    else {
      endFiring(core);

      int xAbs = abs(centerX - faceCenterX);
      int yAbs = abs(centerY - faceCenterY);

      printf("sentry: x-abs %i, y-abs %i\n", xAbs, yAbs);

      if (xAbs > 20 || xAbs > yAbs) {
        // move horizontally
        if (faceCenterX < centerX) {
          // move left
          printf("sentry: moving left\n");
          move(core, MOVE_LEFT);
        }
        else {
          // move right
          printf("sentry: moving right\n");
          move(core, MOVE_RIGHT);
        }
      }
      else {
        // move vertically
        if (faceCenterY < centerY) {
          // move up
          printf("sentry: moving up\n");
          move(core, MOVE_UP);
        }
        else {
          // move down
          printf("sentry: moving down\n");
          move(core, MOVE_DOWN);
        }
      }

      core->moving = true;
      setLedMode(core, LED_BLINK_FAST);
    }

    core->trackingFace = true;
  }
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

  Launcher_t launcher = launcherStart();

  Core core;
  coreInit(&core);
  core.launcher = launcher;
  ledTimerStart(&core);
  firingTimerStart(&core);

  sentryModeChanged(&core);

  Controller_t controller = controllerInit(&core);
  controllerStart(controller);

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
    send(&core, e);
  }
}

