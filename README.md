# thunder

![img](https://d3gqasl9vmjfd8.cloudfront.net/a5b6511c-a539-48c0-881b-6e3d2adcadec.jpg)

A c program that drives a usb missle launcher with manual and sentry (automated) control modes. The manual mode can be driven with a PS4 controller, and the automated mode relies on a usb webcam and opencv to track and recognize faces it should attack. There are fun robot sound effects, it is a blast at a party.

## Getting Started

This project depends on MacOS. It also requires a DreamCheeky Thunder, a PSEye camera taped to the top of it, and a PS4 controller. :)

First, install opencv.
```bash
brew install opencv
```

Then, build with cmake.

```bash
$ cmake .
-- Found OpenCV: /usr/local/Cellar/opencv/4.7.0_2 (found version "4.7.0")
-- Configuring done
-- Generating done
-- Build files have been written to: /Users/michaelhenry/dev/funtastic/branches/thunder

$ make
[  7%] Building CXX object CMakeFiles/capture-ps3eye.dir/ps3eye.cpp.o
[ 15%] Building CXX object CMakeFiles/capture-ps3eye.dir/capture-ps3eye.cpp.o
[ 23%] Linking CXX static library libcapture-ps3eye.a
[ 23%] Built target capture-ps3eye
[ 30%] Building C object CMakeFiles/sound.dir/sound.m.o
[ 38%] Linking C static library libsound.a
[ 38%] Built target sound
[ 46%] Building C object CMakeFiles/core.dir/errors.c.o
[ 53%] Building C object CMakeFiles/core.dir/core.c.o
[ 61%] Building C object CMakeFiles/core.dir/controller.c.o
[ 69%] Building C object CMakeFiles/core.dir/launcher.c.o
[ 76%] Building C object CMakeFiles/core.dir/face-capture.c.o
[ 84%] Linking CXX executable core
[ 84%] Built target core
[ 92%] Building C object CMakeFiles/test.dir/test.m.o
[100%] Linking C executable test
[100%] Built target test
```

Connect the Thunder to your Mac via USB, and connect the PS4 controller. Then, run the `core` binary from the root of the git repo. The program will detect the devices automatically (assuming there aren't more than one of each type of device plugged in). Turn up the sound, so you can hear the sound effects. 

## Usage

This program supports three operating modes:

1. Manual Mode - you can use the dpad to aim the Thunder and the X button to fire. You can fire single shots, or hold down the fire button to fire continuously until all the arrows have been spent. R1 informs the program that the arrows have been reloaded, allowing it to fire again.

2. Sentry Mode - the face-tracking is used to identify and track targets by moving the Thunder automatically. There are sound effects to indicate when a target is identified and lost. The red light on the thunder is on by default, and blinks when tracking.

3. Armed Sentry Mode - the Thunder will track and automatically fire on targets that are identified and tracked until it runds out of arrows. The red light is off by default, but blinks when tracking.

### Controls

* dpad - moves the Thunder up/down, left/right
* cross - fire
* square - toggles Thunder led on and off
* triangle - toggles between manual and sentry mode
* circle - arms/disarms sentry mode
* r1 - tells the program the Thunder has been reloaded

## Design

I sketched some initial ideas [here](https://www.lucidchart.com/documents/view/e6b09b75-3998-4206-8caa-7c4b6ea134c8#).

Inputs:
- controller (mode, manual controls, reload-notify)
- camera (faces)

Outputs:
- controller (led color)
- launcher (movement and firing)

The inputs represent commands that set actions in motion which take time to complete. This means that the state inside the application can be changing over time even though no new inputs have been received (such as completing a fire command, which takes about 3 seconds).

The easiest way I can think of to implement this system is by using pthreads:
- one thread shovels input events from the controller, handling hotplug events as well
- another thread shovels input events from the camera, handling hotplug events as well
- a third thread handles internal scheduling events such as periodically blinking the launcher led or moving it to a specific position (requiring a start-moving and then a stop-moving command sequence with a duration in between).

In this model, each thread would need to obtain an app-global mutex in order to make changes to the application state. Because of this, the actual internal app state changes could happen on any of the three threads. 

### Files

#### core.h
Defines all the events that can be sent into the application core, as well as the send interface. Also defines the functions used by `main.c` to manage the core thread that processes events.

#### core.c
Implements the send interface, which queues events to be processed by the core thread. Manages the core thread which processes the events in queue-order. 

Events have a whenOccurred, which helps the core to know which events to discard in the case they are too old to be useful. For example, a face detected in a frame that is > 200ms old.

Also implements internal timer thread to handle scheduling events such as periodically blinking the launcher led or moving it to a specific position (requiring a start-moving and then a stop-moving command sequence with a duration in between).

#### launcher.h
Defines commands that can be sent to the launcher. The core uses this contract to communicate with the launcher, if one is present. If no launcher is present, the contract returns an error code to indicate this.

#### launcher.c
Implements sending launcher commands, as well as handling launcher hotplug events. Only one launcher can be communicated with at a time, additional launchers will be ignored as only the first discovered launcher is used.

#### controller.h
Defines commands that can be sent to the controller. The core uses this contract to communicate with the controller, if one is present. If no controller is present, the contract returns an error code to indicate this. Also defines the functions used by `main.c` to manage the thread that shovels inputs from the controller.

#### controller.c
Implements sending (synchronous) and receiving (dedicated thread) controller commands, as well as handling controller hotplug events. Only one controller can be communicated with at a time, additional controllers will be ignored as only the first discovered controller is used. Depends on the `event.h` contract to send input events to the core.

#### camera.h
Defines the functions used by `main.c` to manage the thread that shovels inputs from the camera.

#### camera.cpp
Implements receiving (dedicated thread) camera inputs, as well as handling camera hotplug events. Only one camera can be communicated with at a time, additional cameras will be ignored as only the first discovered camera is used. Depends on the `event.h` contract to send input events to the core.

Its worth noting that this is written in c++ and is actually its own separately-linked library because it uses OpenCV for camera image capture and face-detection. OpenCV appears to have deprecated their c bindings and pulled all the docs for them. :(

#### main.c

Wires up `launcher`, `controller` `camera` and `core`, this is the entry point for the entire app. Handles command-line options, etc.

## License

Copyright 2019 Michael Henry

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

