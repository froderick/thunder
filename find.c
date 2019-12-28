#include <stdio.h>
#include <libusb-1.0/libusb.h>

void printdev(libusb_device *dev) {

  struct libusb_device_descriptor desc;
  int r = libusb_get_device_descriptor(dev, &desc);
  if (r < 0) {
    printf("failed to get device descriptor\n");
    return;
  }
  printf("Number of possible configurations: %i\n", desc.bNumConfigurations);
  printf("Device Class: %i\n", desc.bDeviceClass);
  printf("VendorID: %i\n", desc.idVendor);
  printf("ProductID: %i\n", desc.idProduct);

  struct libusb_config_descriptor *config;
  libusb_get_config_descriptor(dev, 0, &config);

  printf("Interfaces: %i\n", config->bNumInterfaces);
  for(int i=0; i<(int)config->bNumInterfaces; i++) {

    const struct libusb_interface *inter;
    const struct libusb_interface_descriptor *interdesc;
    const struct libusb_endpoint_descriptor *epdesc;

    inter = &config->interface[i];
    printf("Number of alternate settings: %i\n", inter->num_altsetting);
    for(int j=0; j<inter->num_altsetting; j++) {
      interdesc = &inter->altsetting[j];
      printf("Interface Number: %i\n", interdesc->bInterfaceNumber);
      printf("Number of endpoints: %i\n", interdesc->bNumEndpoints);
      for(int k=0; k<(int)interdesc->bNumEndpoints; k++) {
        epdesc = &interdesc->endpoint[k];
        printf("Descriptor Type: %i\n", epdesc->bDescriptorType);
        printf("EP Address: %i\n", epdesc->bEndpointAddress);
      }
    }
  }
  printf("\n\n");
  libusb_free_config_descriptor(config);
}

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

// LIBUSB_RECIPIENT_DEVICE
// LIBUSB_ENDPOINT_OUT
// LIBUSB_REQUEST_TYPE_STANDARD

int main() {

  /*
   * spin up a hotplug callback
   * for each hit on the callback:
   *   - spin up new pthreads to manage added devices
   *   - interrupt and tear down existing threads for removed thunder devices and clean up
   *   - managing thunder devices means
   *     - connecting a zmq socket to the cnc api, with the right key, rig identifier (hostname), and encryption turned on
   *     - listening to commands, and executing them on the thunder device
   *   - managing video devices means
   *     - listening
   */

  printf("has hotplug: %i\n", libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG));

  printf("%#08x\n", LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT);

  struct libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
  struct libusb_context *ctx = NULL; //a libusb session
  int r; //for return values
  ssize_t cnt; //holding number of devices in list
  r = libusb_init(&ctx); //initialize a library session
  if(r < 0) {
    printf("Init Error %i\n", r);
    return 1;
  }

  libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);

  //libusb_set_debug(ctx, 3); //set verbosity level to 3, as suggested in the documentation
  cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
  if(cnt < 0) {
    printf("Get Device Error %i\n", cnt);
  }

  ssize_t i; //for iterating through the list
  for(i = 0; i < cnt; i++) {
    struct libusb_device *dev = devs[i];

    if (isThunder(dev)) {

      libusb_device_handle *handle = NULL;
      int open = libusb_open(dev, &handle);
      if (open == 0) {
        printf("opened device\n");
      }
      else {
        printf("failed to open device: %i\n", open);
        return 0;
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

      int active = libusb_kernel_driver_active(handle, ifaceNum);
      printf("kernel driver active: %i\n", active);

      if (active) {
        int detach = libusb_detach_kernel_driver(handle, ifaceNum);
        printf("attempted kernel detatch: %i\n", detach);
      }

      int claim = libusb_claim_interface(handle, ifaceNum);
      if (r != LIBUSB_SUCCESS) {
        printf("claim failed: %i\n", claim);
        return 0;
      }

      uint8_t bmRequestType = 0x21;
      uint8_t bRequest = 0x09;
      uint16_t wValue = 0;
      uint16_t wIndex = 0;
      uint8_t data[8] = {0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      uint16_t wLength = 8;
      unsigned int timeout = 0; // no timeout

      int write = libusb_control_transfer(handle, bmRequestType, bRequest, wValue,
                                          wIndex, data, wLength, timeout);

      if (write < 0) {
        printf("write failed: %i\n", write);
        return 0;
      }

      printdev(dev);

      break;
    }
  }
  libusb_free_device_list(devs, 1); //free the list, unref the devices in it
  libusb_exit(ctx); //close the session
  return 0;
}
