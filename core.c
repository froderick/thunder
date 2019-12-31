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

void handleFace(Core *core, FaceEvent e) {

  // TODO
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
  FaceCapture_t capture = faceCaptureInit(&core);

  core.launcher = launcher;

  controllerStart(controller);
  ledTimerStart(&core);
  firingTimerStart(&core);
  faceCaptureStart(capture);

  while (true) {
    sleep(10);
  }
}

