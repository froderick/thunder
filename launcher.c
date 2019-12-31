#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include "launcher.h"
#include "errors.h"

int isThunder(struct libusb_device *dev) {
  struct libusb_device_descriptor desc;
  int r = libusb_get_device_descriptor(dev, &desc);
  if (r < 0) {
    printf("failed to get device descriptor\n");
    return 0;
  }
  int usbVendor = 0x2123;
  int usbProduct = 0x1010;
  return usbVendor == desc.idVendor && usbProduct == desc.idProduct;
}

typedef struct Launcher {
  struct libusb_context *ctx;
  libusb_device_handle *handle;
} Launcher;

Launcher_t launcherStart() {

  Launcher *l = malloc(sizeof(Launcher));
  if (l == NULL) {
    explode("malloc failed");
  }
  l->ctx = NULL;
  l->handle = NULL;

  printf("has hotplug: %i\n", libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG));

  printf("%#08x\n", LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT);

  struct libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
  int r; //for return values
  ssize_t cnt; //holding number of devices in list
  r = libusb_init(&l->ctx); //initialize a library session
  if(r < 0) {
    explode("init error");
  }

//  libusb_set_option(l->ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);

  cnt = libusb_get_device_list(l->ctx, &devs); //get the list of devices
  if(cnt <= 0) {
    explode("Get Device Error %lu\n", cnt);
  }

  ssize_t i; //for iterating through the list
  bool found = false;
  for(i = 0; i < cnt; i++) {
    struct libusb_device *dev = devs[i];

    if (isThunder(dev)) {

      found = true;

      l->handle = NULL;
      int open = libusb_open(dev, &l->handle);
      if (open == 0) {
        printf("opened device\n");
      }
      else {
        explode("failed to open device: %i\n", open);
      }

      struct libusb_config_descriptor *confDesc;
      int getdesc = libusb_get_config_descriptor(dev, 0, &confDesc);
      if (getdesc != 0) {
        printf("get config failed: %i\n", getdesc);
        return 0;
      }
      printf("found %i interfaces\n", confDesc->bNumInterfaces);
      printf("first interface number: %i\n", confDesc->interface[0].altsetting[0].bInterfaceNumber);

      int ifaceNum = confDesc->interface[0].altsetting[0].bInterfaceNumber;

      int active = libusb_kernel_driver_active(l->handle, ifaceNum);
      printf("kernel driver active: %i\n", active);

      if (active) {
        int detach = libusb_detach_kernel_driver(l->handle, ifaceNum);
        printf("attempted kernel detatch: %i\n", detach);
      }

      int claim = libusb_claim_interface(l->handle, ifaceNum);
      if (r != LIBUSB_SUCCESS) {
        printf("claim failed: %i\n", claim);
        return 0;
      }

      break;
    }
  }
  libusb_free_device_list(devs, 1); //free the list, unref the devices in it

  if (!found) {
    explode("thunder not found");
  }

  return l;
}

void launcherStop(Launcher *l) {
  libusb_exit(l->ctx); //close the session
}

uint8_t commands[][8] = {
    {0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_DOWN
    {0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_UP
    {0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_LEFT
    {0x02, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_RIGHT
    {0x02, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_FIRE
    {0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_STOP
    {0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_LEDON
    {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // C_LEDOFF
};

bool launcherSend(Launcher *l, LauncherCmd cmd) {

  uint8_t bmRequestType = 0x21;
  uint8_t bRequest = 0x09;
  uint16_t wValue = 0;
  uint16_t wIndex = 0;
  uint8_t *data = commands[cmd];
  uint16_t wLength = 8;
  unsigned int timeout = 0; // no timeout

  int write = libusb_control_transfer(l->handle, bmRequestType, bRequest, wValue,
                                      wIndex, data, wLength, timeout);

  if (write < 0) {
    printf("write failed: %i\n", write);
    return false;
  }

  return true;
}


