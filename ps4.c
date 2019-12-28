#include <IOKit/hid/IOHIDValue.h>
#include <IOKit/hid/IOHIDManager.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

pthread_mutex_t mutex;

void sendHeartbeat() {
  pthread_mutex_lock(&mutex);
  printf("{\"type\": \"heartbeat\"}\n");
  fflush(stdout);
  pthread_mutex_unlock(&mutex);
}

void sendControl(char *name, char *value) {
  pthread_mutex_lock(&mutex);
  printf("{\"type\": \"control\",  \"name\": \"%s\", \"value\": \"%s\"}\n", name, value);
  fflush(stdout);
  pthread_mutex_unlock(&mutex);
}

void controllerInputCallback( void* context,  IOReturn result,  void* sender,  IOHIDValueRef value ) {

  IOHIDElementRef element = IOHIDValueGetElement(value);
  IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
  const int int_value = IOHIDValueGetIntegerValue(value);

  if (cookie == 2) { // square
    if (int_value == 1) {
      sendControl("led-toggle", "down");
    }
    else {
      sendControl("led-toggle", "up");
    }
  }
  else if (cookie == 3) { // X
    if (int_value == 1) {
      sendControl("fire", "down");
    }
    else {
      sendControl("fire", "up");
    }
  }
  else if (cookie == 20) { // d-pad
    switch (int_value) {
      case 0:
        sendControl("direction", "up");
        break;
      case 4:
        sendControl("direction", "down");
        break;
      case 6:
        sendControl("direction", "left");
        break;
      case 2:
        sendControl("direction", "right");
        break;
      case 8:
        sendControl("direction", "none");
        break;
    }
    fflush(stdout);
  }
}

int kHidPageDesktop = kHIDPage_GenericDesktop;
int kHidUsageGamepad = kHIDUsage_GD_GamePad;
int kHidUsageJoystick = kHIDUsage_GD_Joystick;
int kHidUsageController = kHIDUsage_GD_MultiAxisController;

static void attachCallback(void *context, IOReturn r, void *hidManager, IOHIDDeviceRef device)
{
  IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
  IOHIDDeviceRegisterInputValueCallback(device, controllerInputCallback, NULL);
  IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
}

int main(void) {

  pthread_mutex_init(&mutex, NULL);

  // schedule hearbeat
  {
    // https://stackoverflow.com/questions/44807302/create-c-timer-in-macos/52905687#52905687
    dispatch_queue_t queue = dispatch_queue_create("timerQueue", 0);
    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    dispatch_source_set_event_handler(timer, ^{ sendHeartbeat(); });

    dispatch_time_t start = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5); // 20 seconds
    dispatch_source_set_timer(timer, start, NSEC_PER_SEC * 20, 0);  // 20 seconds
    dispatch_resume(timer);
  }

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
  IOHIDManagerRegisterDeviceMatchingCallback(hidManager, attachCallback, NULL);
  IOHIDManagerScheduleWithRunLoop( hidManager, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
  IOHIDManagerOpen( hidManager, kIOHIDOptionsTypeNone );
  IOHIDManagerScheduleWithRunLoop(hidManager, CFRunLoopGetCurrent(), CFSTR("RunLoopModeDiscovery"));

  CFRunLoopRun(); // spins
}