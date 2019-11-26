#include <IOKit/hid/IOHIDValue.h>
#include <IOKit/hid/IOHIDManager.h>
#include <pthread.h>

void msleep(uint64_t ms) {
  usleep(ms * 1000);
}

uint64_t now() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t millis = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  return millis;
}

pthread_mutex_t mutex;
bool firingEnabled;
uint64_t firingStartInstant;
bool firingStopScheduled;
uint64_t firingEndInstant;
bool ledOn = false;

// TODO: really need a timer to keep sending fire when the first fire command expires and the fire button is still down
// https://github.com/nevali/opencflite/blob/master/examples/CFRunLoopTimerExample/CFRunLoopTimerExample.c

void myHIDKeyboardCallback( void* context,  IOReturn result,  void* sender,  IOHIDValueRef value ) {

  pthread_mutex_lock(&mutex);

  IOHIDElementRef element = IOHIDValueGetElement(value);
  IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
  const int int_value = IOHIDValueGetIntegerValue(value);

  if (firingStopScheduled) {
    uint64_t t = now();
    if (t >= firingEndInstant) {
      printf("disabling firing, schedule met\n");
      firingEnabled = false;
      firingStopScheduled = false;
    }
  }

  if (cookie == 2) { // square
    if (int_value == 1) {
      if (ledOn) {
        ledOn = false;
        printf("ledoff\n");
      }
      else {
        ledOn = true;
        printf("ledon\n");
      }
    }
//    switch (int_value) {
//      case 0:
//        printf("ledoff\n");
//        break;
//      case 1:
//        printf("ledon\n");
//        break;
//      default:
//        printf("dunno: cookie=%i, value=%i\n", cookie, int_value);
//    }
    fflush(stdout);
  }
  else if (firingEnabled) {
    if (cookie == 3) { // X
      switch (int_value) {
        case 0:
          if (!firingStopScheduled) {
            uint64_t t = now();
            firingEndInstant = firingStartInstant;
            while (firingEndInstant < t) {
              firingEndInstant += 3300;
            }
            firingStopScheduled = true;
            int sleep = firingEndInstant - t;
            printf("sleep %i\n", sleep);
            printf("start %" PRIu64 ", end %" PRIu64 ", now %" PRIu64 "\n", firingStartInstant, firingEndInstant, t);

            printf("stop\n");
            fflush(stdout);
          }
          break;
        case 1:
          printf("ignoring fire, waiting for firing to become disabled again");
          break;
        default:
          printf("dunno: cookie=%i, value=%i\n", cookie, int_value);
      }
    }
  }
  else {
    if (cookie == 20) {
      switch (int_value) {
        case 0:
          printf("up\n");
          break;
        case 4:
          printf("down\n");
          break;
        case 6:
          printf("left\n");
          break;
        case 2:
          printf("right\n");
          break;
        case 8:
          printf("stop\n");
          break;
        default:
          printf("dunno: cookie=%i, value=%i\n", cookie, int_value);
      }
      fflush(stdout);
    }
    else if (cookie == 3) { // X
      switch (int_value) {
        case 1:
          firingEnabled = true;
          firingStartInstant = now();
          firingStopScheduled = false;
          printf("fire\n");
          break;
        default:
          printf("dunno: cookie=%i, value=%i\n", cookie, int_value);
      }
      fflush(stdout);
    }
  }
//   else {
//     switch (int_value) {
//       case 121:
//       case 122:
//       case 123:
//       case 124:
//       case 125:
//       case 126:
//       case 127:
//       case 128:
//       case 129:
//       case 130:
//       case 131:
//       case 132:
//         break;
//       default:
//         printf("got callback: cookie=%i, value=%i\n", cookie, int_value);
//     }
//   }
  pthread_mutex_unlock(&mutex);
}

int kHidPageDesktop = kHIDPage_GenericDesktop;
int kHidUsageGamepad = kHIDUsage_GD_GamePad;
int kHidUsageJoystick = kHIDUsage_GD_Joystick;
int kHidUsageController = kHIDUsage_GD_MultiAxisController;

static void attach_callback(void *context, IOReturn r, void *hidManager, IOHIDDeviceRef device)
{
  // Get device name.
//  char* device_name;
//  CFTypeRef nameRef = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
//  if (nameRef == NULL || CFGetTypeID(nameRef) != CFStringGetTypeID()) {
//    device_name = "<Unknown>";
//  } else {
//    char buffer[1024];
//    CFStringGetCString((CFStringRef)nameRef, buffer, 1024, kCFStringEncodingUTF8);
//    device_name = buffer;
//  }
//
//  printf("device name: %s\n", device_name);

  // Open HID device and attach input callback.
  IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
  IOHIDDeviceRegisterInputValueCallback(device, myHIDKeyboardCallback, NULL);
  // Schedule event handling on a separate thread.
  IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
}

int main(void)
{
  pthread_mutex_init(&mutex, NULL);
  firingEnabled = false;

  IOHIDManagerRef hidManager = IOHIDManagerCreate( kCFAllocatorDefault, kIOHIDOptionsTypeNone );

// Create device matching dictionary.
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

  // Set the dictionary.
  IOHIDManagerSetDeviceMatchingMultiple(hidManager, dictionariesRef);

//  IOHIDManagerRegisterInputValueCallback( hidManager, myHIDKeyboardCallback, NULL );
  IOHIDManagerRegisterDeviceMatchingCallback(hidManager, attach_callback, NULL);

  IOHIDManagerScheduleWithRunLoop( hidManager, CFRunLoopGetMain(), kCFRunLoopDefaultMode );

  IOHIDManagerOpen( hidManager, kIOHIDOptionsTypeNone );

  IOHIDManagerScheduleWithRunLoop(hidManager, CFRunLoopGetCurrent(), CFSTR("RunLoopModeDiscovery"));

  CFRunLoopRun(); // spins
}