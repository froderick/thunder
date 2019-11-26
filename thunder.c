#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <stdlib.h>
#include <stdio.h>
#include <mach/mach_port.h>
#include <CoreFoundation/CoreFoundation.h>
#include "errors.h"

// general USB device discovery and open

IOUSBDeviceInterface** openDevice(SInt32 usbVendor, SInt32 usbProduct) {

  CFMutableDictionaryRef  matchingDict;
  kern_return_t           kr;
  io_service_t            usbDevice;
  IOCFPlugInInterface     **plugInInterface = NULL;
  IOUSBDeviceInterface    **dev = NULL;
  HRESULT                 result;
  SInt32                  score;

  matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
  if (!matchingDict) {
    explode("couldn’t create a USB matching dictionary\n");
  }

  CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorName),
                       CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor));
  CFDictionarySetValue(matchingDict, CFSTR(kUSBProductName),
                       CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbProduct));

  io_iterator_t iterator;
  kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iterator);
  if (kr != KERN_SUCCESS) {
    explode("failed to list matching devices: %i", kr);
  }

  usbDevice = IOIteratorNext(iterator);

  if (!usbDevice) {
    explode("can't find a matching device");
  }

  kr = IOCreatePlugInInterfaceForService(usbDevice,
                                         kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
                                         &plugInInterface, &score);
  if ((kIOReturnSuccess != kr) || !plugInInterface) {
    explode("unable to create a plug-in (%08x)\n", kr);
  }

  kr = IOObjectRelease(usbDevice); // don’t need the device object after intermediate plug-in is created
  if (kr != kIOReturnSuccess) {
    explode("unable to release usb device: %08x\n", kr);
  }

  result = (*plugInInterface)->QueryInterface(plugInInterface,
                                              CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                              (LPVOID *)&dev);
  if (result || !dev) {
    explode("couldn’t create a device interface (%08x)\n", (int) result);
  }
  (*plugInInterface)->Release(plugInInterface); // don’t need the intermediate plug-in after device interface is created

  kr = (*dev)->USBDeviceOpen(dev);
  if (kr != kIOReturnSuccess) {
    explode("unable to open device: %08x\n", kr);
  }

  return dev;
}

void closeDevice(IOUSBDeviceInterface **dev) {
  kern_return_t kr;
  kr = (*dev)->USBDeviceClose(dev);
  if (kr != kIOReturnSuccess) {
    explode("failed to close usb device: 0x%x\n", kr);
  }
  kr = (*dev)->Release(dev);
  if (kr != kIOReturnSuccess) {
    explode("failed to release usb device: 0x%x\n", kr);
  }
}

// thunder protocol

/*
 * the led is independent of the motor?
 */

typedef enum Cmd {
  C_DOWN,
  C_UP,
  C_LEFT,
  C_RIGHT,
  C_FIRE,
  C_STOP,
  C_LEDON,
  C_LEDOFF,
} Cmd;

UInt8 commands[][8] = {
    {0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_DOWN
    {0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_UP
    {0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_LEFT
    {0x02, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_RIGHT
    {0x02, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_FIRE
    {0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_STOP
    {0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_LEDON
    {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_LEDOFF
};

void writeDevice(IOUSBDeviceInterface **dev, Cmd cmd) {

  IOUSBDevRequest request;
  request.bmRequestType = 0x21;
  request.bRequest = 0x09;
  request.wValue = 0;
  request.wIndex = 0;
  request.wLength = 8;
  request.pData = commands[cmd];

  IOReturn ret = (*dev)->DeviceRequest(dev, &request);

  if (ret != kIOReturnSuccess) {
    explode("WriteToDevice reset returned err 0x%x\n", ret);
  }
}

void down(IOUSBDeviceInterface **dev) {

}

void msleep(uint64_t ms) {
  usleep(ms * 1000);
}

typedef struct Thunder {
  IOUSBDeviceInterface **dev;
  uint64_t stepsX, stepsY; // steps are defined in 15ms increments
  bool ledOn;
} Thunder;

#define X_NUM_STEPS 230
#define X_STEP_DURATION_MILLIS 15
#define X_NUM_DEGREES 270

/*
 * 19 steps down at 15ms
 * 21 steps up at 15ms
 *
 * 32 steps right at 100ms, 55 steps right at 50ms, 111 steps right at 25 ms, 230 steps right at 15 ms
 * 270-degree turn radius
 */
void moveTo(Thunder *t, int degreesX, int degreesY) {

  if (degreesX < -135) {
    printf("can't go less than -135 for x, rounding to minimum\n");
    degreesX = -135;
  }
  if (degreesX > 135) {
    printf("can't go more than 135 for x, rounding to maximum\n");
    degreesX = 135;
  }

  int degreesXNonNegative = degreesX + 135; // convert to non-negative degrees
  int stepsXTarget = (int) (((float)degreesXNonNegative / X_NUM_DEGREES) * X_NUM_STEPS);

  if (t->stepsX > stepsXTarget) {

    int numSteps = t->stepsX - stepsXTarget;
    int durationMillis = numSteps * X_STEP_DURATION_MILLIS;
    printf("moving left %i steps, %i ms\n", numSteps, durationMillis);

    writeDevice(t->dev, C_LEFT);
    msleep(durationMillis);
    writeDevice(t->dev, C_STOP);
  }
  else if (t->stepsX == stepsXTarget) {
    // done
  }
  else {

    int numSteps = stepsXTarget - t->stepsX;
    int durationMillis = numSteps * X_STEP_DURATION_MILLIS;
    printf("moving right %i steps, %i ms\n", numSteps, durationMillis);

//    writeDevice(t->dev, C_RIGHT);
//    msleep(durationMillis);
//    writeDevice(t->dev, C_STOP);

    for (int i=0; i<numSteps; i++) {
      writeDevice(t->dev, C_RIGHT);
      msleep(15);
      writeDevice(t->dev, C_STOP);
      msleep(500);
    }
  }
}

/*
 * fire takes 3300 ms
 */
void fire(Thunder *t) {
  writeDevice(t->dev, C_FIRE);
  msleep(3300);
}

void thunderInitContents(Thunder *t) {
  t->dev = NULL;
  t->stepsX = 0;
  t->stepsY = 0;
  t->ledOn = false;
}

void thunderInit(Thunder *t) {

  thunderInitContents(t);

  // set up usb device

  SInt32 usbVendor = 0x2123;
  SInt32 usbProduct = 0x1010;
  t->dev = openDevice(usbVendor, usbProduct);

  // calibration sequence

  writeDevice(t->dev, C_LEDOFF);

  writeDevice(t->dev, C_LEFT);
  msleep(6000);

  writeDevice(t->dev, C_DOWN);
  msleep(1000);

  moveTo(t, 0, 0);
}

void thunderFreeContents(Thunder *t) {
  closeDevice(t->dev);
  thunderInitContents(t);
}

int main() {

   SInt32 usbVendor = 0x2123;
   SInt32 usbProduct = 0x1010;
   IOUSBDeviceInterface **dev = openDevice(usbVendor, usbProduct);

  char *buffer;
  size_t bufsize = 32;
  size_t charsRead;

  buffer = (char *)malloc(bufsize * sizeof(char));
  if( buffer == NULL)
  {
    explode("Unable to allocate buffer");
  }

  char *sleepPrefix = "sleep ";
  size_t sleepPrefixLength = strlen(sleepPrefix);

  while (true) {
    charsRead = getline(&buffer, &bufsize, stdin);
    if (charsRead == -1) {
      break;
    }

    // ignore newline
    buffer[charsRead - 1] = '\0';
    charsRead -= 1;

//    printf("%zu characters were read.\n", charsRead);
//    printf("You typed: '%s'\n", buffer);

    if (strncmp("down", buffer, charsRead) == 0) {
      writeDevice(dev, C_DOWN);
    }
    else if (strncmp("up", buffer, charsRead) == 0) {
      writeDevice(dev, C_UP);
    }
    else if (strncmp("left", buffer, charsRead) == 0) {
      writeDevice(dev, C_LEFT);
    }
    else if (strncmp("right", buffer, charsRead) == 0) {
      writeDevice(dev, C_RIGHT);
    }
    else if (strncmp("fire", buffer, charsRead) == 0) {
      writeDevice(dev, C_FIRE);
    }
    else if (strncmp("stop", buffer, charsRead) == 0) {
      writeDevice(dev, C_STOP);
    }
    else if (strncmp("ledon", buffer, charsRead) == 0) {
      writeDevice(dev, C_LEDON);
    }
    else if (strncmp("ledoff", buffer, charsRead) == 0) {
      writeDevice(dev, C_LEDOFF);
    }
    else if (strncmp(sleepPrefix, buffer, sleepPrefixLength) == 0) {

      char *sleepMillis = buffer + sleepPrefixLength;

      errno = 0;
      uint64_t value = strtoull(sleepMillis, NULL, 0);
      if (errno == EINVAL) {
        explode("cannot read millis: %s", buffer);
      }
      if (errno == ERANGE) {
        explode("cannot read millis: %s", buffer);
      }

      msleep(value);
    }
    else {
      printf("unknown command: %s\n", buffer);
      continue;
    }
  }

  printf("quitting\n");
}

// int main (int argc, const char *argv[])
// {
// //  Thunder t;
// //  thunderInit(&t);
//
//   SInt32 usbVendor = 0x2123;
//   SInt32 usbProduct = 0x1010;
//   IOUSBDeviceInterface **dev = openDevice(usbVendor, usbProduct);
//
//   writeDevice(dev, C_LEDOFF);
//   msleep(200);
//   writeDevice(dev, C_LEDON);
//
//   writeDevice(dev, C_LEFT); // left
//   msleep(6000);
//   writeDevice(dev, C_RIGHT); // center
//
//   for (int i=0; i<40; i++) {
//     writeDevice(dev, C_LEDON);
//     msleep(200);
//     writeDevice(dev, C_LEDOFF);
//     msleep(200);
//   }
//
//   exit(0);
//
//   // firing sequence
// //  writeDevice(dev, C_RIGHT);
// //  msleep(2600);
// //  writeDevice(dev, C_STOP);
// //  for (int i=0; i<4; i++) {
// //    writeDevice(dev, C_FIRE);
// //    msleep(3300);
// //  }
//
//   /*
//    * left to center
//    */
//
// //  writeDevice(dev, C_LEFT);
// //  msleep(6000);
// //
// //  writeDevice(dev, C_RIGHT);
// //  msleep(2575);
// //  writeDevice(dev, C_STOP);
//
//   /*
//    * right to center
//    */
//
// //  writeDevice(dev, C_RIGHT);
// //  msleep(6000);
// //
// //  writeDevice(dev, C_LEFT);
// //  msleep(2500);
// //  writeDevice(dev, C_STOP);
//
//   /*
//    * center calibration
//    */
//
//   writeDevice(dev, C_LEFT); // left
//   msleep(6000);
//   writeDevice(dev, C_RIGHT); // center
//   msleep(2600);
//   writeDevice(dev, C_STOP);
//   msleep(500);
//
//   /*
//    * range tests
//    */
//
//   writeDevice(dev, C_LEFT); // -90
//   msleep(1667);
//   writeDevice(dev, C_STOP);
//   msleep(500);
//
//   writeDevice(dev, C_RIGHT); // 0
//   msleep(1667);
//   writeDevice(dev, C_STOP);
//   msleep(500);
//
//   writeDevice(dev, C_LEFT); // left
//   msleep(2500);
//   writeDevice(dev, C_STOP);
//   msleep(500);
//
//   writeDevice(dev, C_RIGHT); // center
//   msleep(2500);
//   writeDevice(dev, C_STOP);
//   msleep(500);
//
//   writeDevice(dev, C_RIGHT); // right
//   msleep(2500);
//   writeDevice(dev, C_STOP);
//   msleep(500);
//
//   writeDevice(dev, C_LEFT); // center
//   msleep(2500);
// //  msleep(2500 - (2500 * 0.035));
//   writeDevice(dev, C_STOP);
//   msleep(500);
//
//   writeDevice(dev, C_RIGHT); // 90
//   msleep(1667);
//   writeDevice(dev, C_STOP);
//   msleep(500);
//
//   writeDevice(dev, C_LEFT); // 0
//   msleep(1667);
// //  msleep(1667 - (1667 * 0.035));
//   writeDevice(dev, C_STOP);
//   msleep(500);
//
//   exit(0);
//
// //  writeDevice(dev, C_RIGHT); // right
// //  msleep(2575);
// //  writeDevice(dev, C_STOP);
// //  msleep(500);
// //
// //  writeDevice(dev, C_LEFT); // center
// //  msleep(2575 - (2575 * 0.0291));
// //  writeDevice(dev, C_STOP);
// //  msleep(500);
// //
// //  writeDevice(dev, C_LEFT); // left
// //  msleep(2575 - (2575 * 0.0291));
// //  writeDevice(dev, C_STOP);
// //  msleep(500);
//
// //  writeDevice(dev, C_RIGHT); // center
// //  msleep(2575);
// //  writeDevice(dev, C_STOP);
// //  msleep(500);
// //
// //  writeDevice(dev, C_LEFT); // half-left
// //  msleep(1287 - (1287 * 0.0291));
// //  writeDevice(dev, C_STOP);
// //  msleep(500);
// //
// //  writeDevice(dev, C_RIGHT); // center
// //  msleep(1287);
// //  writeDevice(dev, C_STOP);
// //  msleep(500);
// //
// //  writeDevice(dev, C_RIGHT); // half-right
// //  msleep(1287);
// //  writeDevice(dev, C_STOP);
// //  msleep(500);
// //
// //  writeDevice(dev, C_LEFT); // center
// //  msleep(1287 - (1287 * 0.0291));
// //  writeDevice(dev, C_STOP);
// //  msleep(500);
//
//
//
//
// //  for (int i=0; i<320; i++) {
// //    writeDevice(dev, C_RIGHT);
// //    msleep(15);
// //    writeDevice(dev, C_STOP);
// //    msleep(500);
// //  }
//
// //  writeDevice(dev, C_UP);
// //  msleep(1000);
// //
// //  for (int i=0; i<20; i++) {
// //    writeDevice(dev, C_DOWN);
// //    msleep(15);
// //    writeDevice(dev, C_STOP);
// //    msleep(500);
// //  }
// //
// //  writeDevice(dev, C_LEDOFF);
// //  msleep(500);
// //  writeDevice(dev, C_LEDON);
// //
// //  for (int i=0; i<30; i++) {
// //    writeDevice(dev, C_UP);
// //    msleep(15);
// //    writeDevice(dev, C_STOP);
// //    msleep(500);
// //  }
//
//
//
//
//
//
//
//
//
//
//
//
//
// //  writeDevice(dev, C_RIGHT);
// //  usleep(2000 * 1000);
// //
// //  writeDevice(dev, C_DOWN);
// //  usleep(800 * 1000);
// //
// //  writeDevice(dev, C_UP);
// //  usleep(800 * 1000);
//
// //  writeDevice(dev, C_FIRE);
// //  usleep(3300 * 1000);
//
//   closeDevice(dev);
//
// //  thunderFreeContents(&t);
//   printf("finished\n");
//   return 0;
// }
