#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "core.h"
#include "errors.h"
#include "controller.h"
#include <inttypes.h>
#include <math.h>

void msleep(uint64_t ms) {
  usleep(ms * 1000);
}

uint64_t now() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t millis = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  return millis;
}

// https://config9.com/linux/how-to-get-the-current-time-in-milliseconds-from-c-in-linux/
void print_current_time_with_ms () {
  long            ms; // Milliseconds
  time_t          s;  // Seconds
  struct timespec spec;

  clock_gettime(CLOCK_REALTIME, &spec);

  s  = spec.tv_sec;
  ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
  if (ms > 999) {
    s++;
    ms = 0;
  }

  uint64_t n = (s * 1000) + ms;

  printf("Current time: %"PRIu64" ms since the Epoch\n", n);
}

#define FIRING_MAX_CAPACITY 4

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
  ControlMode mode;
  uint8_t remainingShots;
  LedMode ledMode;
  SentryMode sentryMode;
} Core;

void coreInit(Core *c) {
  pthread_mutex_init(&c->mutex, NULL);
  c->mode = MODE_MANUAL;
  c->remainingShots = FIRING_MAX_CAPACITY;
  c->ledMode = LED_OFF;
  c->sentryMode = SENTRY_MODE_PASSIVE;
}

void toggleMode(Core *core) {
  // TODO
}

void reloaded(Core *core) {
  core->remainingShots = FIRING_MAX_CAPACITY;
}

void toggleLed(Core *core) {
  switch (core->ledMode) {
    case LED_OFF:
      core->ledMode = LED_ON;
      // TODO: send on command to launcher
      break;
    case LED_ON:
      core->ledMode = LED_BLINK; // scheduler will pick this up
      break;
    case LED_BLINK:
      core->ledMode = LED_OFF;
      // TODO: send off command to launcher
      break;
    default:
      explode("unknown led mode");
  }
}

void beginFiring(Core *core) {

}

void endFiring(Core *core) {

}

void move(Core *core, Movement m) {

}

void toggleSentryMode(Core *core) {

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

}

void handleTimer(Core *core, TimerType t) {

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
    case E_TIMER:
      printf("{\"type\": \"timer\",  \"whenOccurred\": \"%" PRIu64 "\"}\n", e.whenOccurred);
      break;
    default:
    explode("unknown event type: %u\n", e.type);
  }
  fflush(stdout);
}

bool send(Core *core, Event e) {
  pthread_mutex_lock(&core->mutex);

  printEvent(e);

  switch (e.type) {
    case E_CONTROL:
      handleControl(core, e.control);
      break;
    case E_FACE:
      handleControl(core, e.control);
      break;
    case E_TIMER:
      handleTimer(core, e.timer);
      break;
    default:
      printf("error: unknown event type encountered, %u\n", e.type);
      break;
  }

  pthread_mutex_unlock(&core->mutex);
  return true;
}

int main() {

  print_current_time_with_ms();

  Core core;
  coreInit(&core);

  Controller_t controller = controllerStart(&core);
}