#include <IOKit/hid/IOHIDValue.h>
#include <IOKit/hid/IOHIDManager.h>

void controllerInputCallback( void* context,  IOReturn result,  void* sender,  IOHIDValueRef value ) {

  IOHIDElementRef element = IOHIDValueGetElement(value);
  IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
  const int int_value = IOHIDValueGetIntegerValue(value);

  if (cookie == 2) { // square
    if (int_value == 1) {
      printf("{\"control\": \"led-toggle\", \"state\": \"down\"}\n");
    }
    else {
      printf("{\"control\": \"led-toggle\", \"state\": \"up\"}\n");
    }
    fflush(stdout);
  }
  else if (cookie == 3) { // X
    if (int_value == 1) {
      printf("{\"control\": \"fire\", \"state\": \"down\"}\n");
    }
    else {
      printf("{\"control\": \"fire\", \"state\": \"up\"}\n");
    }
    fflush(stdout);
  }
  else if (cookie == 20) { // d-pad
    switch (int_value) {
      case 0:
        printf("{\"control\": \"direction\", \"state\": \"up\"}\n");
        break;
      case 4:
        printf("{\"control\": \"direction\", \"state\": \"down\"}\n");
        break;
      case 6:
        printf("{\"control\": \"direction\", \"state\": \"left\"}\n");
        break;
      case 2:
        printf("{\"control\": \"direction\", \"state\": \"right\"}\n");
        break;
      case 8:
        printf("{\"control\": \"direction\", \"state\": \"none\"}\n");
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