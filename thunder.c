#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <stdlib.h>
#include <stdio.h>
#include <mach/mach_port.h>
#include <CoreFoundation/CoreFoundation.h>
#include "errors.h"

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

void msleep(uint64_t ms) {
  usleep(ms * 1000);
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
    else if (strncmp(sleepPrefix, buffer, sleepPrefixLength) == 0) { // TODO: this does not belong here

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
}

