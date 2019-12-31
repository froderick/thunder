#include <IOKit/hid/IOHIDValue.h>
#include <IOKit/hid/IOHIDManager.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "controller.h"
#include "core.h"
#include "errors.h"

typedef struct Controller {
  Core_t core;
} Controller;

void sendControl(Core_t core, ControlEvent e) {
  send(core, (Event){.type = E_CONTROL, .whenOccurred = now(), .control = e});
}

#define BUTTON_ID_SQUARE 2
#define BUTTON_ID_CROSS 3
#define BUTTON_ID_CIRCLE 4
#define BUTTON_ID_TRIANGLE 5
#define BUTTON_ID_R1 7
#define BUTTON_ID_DPAD 20

void controllerInputCallback( void* context,  IOReturn result,  void* sender,  IOHIDValueRef value ) {

  Controller *c = (Controller*)context;
  Core_t core = c->core;

  IOHIDElementRef element = IOHIDValueGetElement(value);
  IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
  const int int_value = IOHIDValueGetIntegerValue(value);

  if (cookie == BUTTON_ID_SQUARE) {
    if (int_value == 1) {
      sendControl(core, (ControlEvent){.type = CONTROL_TYPE_LED_TOGGLE}); // only send down
    }
    else {
      // don't send up
    }
  }
  else if (cookie == BUTTON_ID_TRIANGLE) {
    if (int_value == 1) {
      sendControl(core, (ControlEvent){.type = CONTROL_TYPE_MODE_TOGGLE}); // only send down
    }
    else {
      // don't send up
    }
  }
  else if (cookie == BUTTON_ID_CIRCLE) {
    if (int_value == 1) {
      sendControl(core, (ControlEvent){.type = CONTROL_TYPE_SENTRY_MODE_TOGGLE}); // only send down
    }
    else {
      // don't send up
    }
  }
  else if (cookie == BUTTON_ID_R1) {
    if (int_value == 1) {
      sendControl(core, (ControlEvent){.type = CONTROL_TYPE_RELOAD}); // only send down
    }
    else {
      // don't send up
    }
  }
//  else if (cookie > 0 && cookie < 10) { // square
//    printf("%u\n", cookie);
//  }
  else if (cookie == BUTTON_ID_CROSS) {
    if (int_value == 1) {
      sendControl(core, (ControlEvent){.type = CONTROL_TYPE_FIRE_BEGIN});
    }
    else {
      sendControl(core, (ControlEvent){.type = CONTROL_TYPE_FIRE_END});
    }
  }
  else if (cookie == BUTTON_ID_DPAD) {
    switch (int_value) {
      case 0:
        sendControl(core, (ControlEvent){.type = CONTROL_TYPE_MOVEMENT, .movement = MOVE_UP});
        break;
      case 4:
        sendControl(core, (ControlEvent){.type = CONTROL_TYPE_MOVEMENT, .movement = MOVE_DOWN});
        break;
      case 6:
        sendControl(core, (ControlEvent){.type = CONTROL_TYPE_MOVEMENT, .movement = MOVE_LEFT});
        break;
      case 2:
        sendControl(core, (ControlEvent){.type = CONTROL_TYPE_MOVEMENT, .movement = MOVE_RIGHT});
        break;
      case 8:
        sendControl(core, (ControlEvent){.type = CONTROL_TYPE_MOVEMENT, .movement = MOVE_NONE});
        break;
    }
  }
}

int kHidPageDesktop = kHIDPage_GenericDesktop;
int kHidUsageGamepad = kHIDUsage_GD_GamePad;
int kHidUsageJoystick = kHIDUsage_GD_Joystick;
int kHidUsageController = kHIDUsage_GD_MultiAxisController;

static void attachCallback(void *context, IOReturn r, void *hidManager, IOHIDDeviceRef device) {
  Controller *c = (Controller*)context;
  IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
  IOHIDDeviceRegisterInputValueCallback(device, controllerInputCallback, c);
  IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
}

void* controllerThread(void *arg) {

  Controller *c = (Controller*)arg;

  printf("thread starting\n");

  IOHIDManagerRef hidManager = IOHIDManagerCreate( kCFAllocatorDefault, kIOHIDOptionsTypeNone );

  CFStringRef keys[2];
  keys[0] = CFSTR(kIOHIDDeviceUsagePageKey);
  keys[1] = CFSTR(kIOHIDDeviceUsageKey);

  CFDictionaryRef dictionaries[3];
  CFNumberRef values[2];
  values[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &kHidPageDesktop);
  values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &kHidUsageJoystick);
  dictionaries[0] = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys, (const void **)values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFRelease(values[0]);
  CFRelease(values[1]);

  values[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &kHidPageDesktop);
  values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &kHidUsageGamepad);
  dictionaries[1] = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys, (const void **)values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFRelease(values[0]);
  CFRelease(values[1]);

  values[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &kHidPageDesktop);
  values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &kHidUsageController);
  dictionaries[2] = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys, (const void **)values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFRelease(values[0]);
  CFRelease(values[1]);

  CFArrayRef dictionariesRef = CFArrayCreate(kCFAllocatorDefault,
                                             (const void **)dictionaries, 3, &kCFTypeArrayCallBacks);
  CFRelease(dictionaries[0]);
  CFRelease(dictionaries[1]);
  CFRelease(dictionaries[2]);

  IOHIDManagerSetDeviceMatchingMultiple(hidManager, dictionariesRef);
  IOHIDManagerRegisterDeviceMatchingCallback(hidManager, attachCallback, c);
  IOHIDManagerScheduleWithRunLoop( hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );

  IOHIDManagerOpen( hidManager, kIOHIDOptionsTypeNone );

  printf("thread spinning\n");

  CFRunLoopRun(); // spins

  printf("thread ending\n");

  return NULL;
}

Controller* controllerInit(Core_t core) {
  Controller *c = malloc(sizeof(Controller));
  if (c == NULL) {
    explode("failed to malloc");
  }
  c->core = core;
  return c;
}

void controllerStart(Controller *c) {

  pthread_t threadId;
  printf("before thread\n");
  pthread_create(&threadId, NULL, controllerThread, c);
  printf("after thread\n");
}

// https://stackoverflow.com/questions/1363787/is-it-safe-to-call-cfrunloopstop-from-another-thread
void controllerStop(Controller_t c) {
  // TODO: create an additional source for the run loop that can be used to trigger run loop termination code in a callback
}


//void printControl(char *name, char *value) {
//  printf("{\"type\": \"control\",  \"name\": \"%s\", \"value\": \"%s\"}\n", name, value);
//  fflush(stdout);
//}
