#ifndef THUNDER_LAUNCHER_H
#define THUNDER_LAUNCHER_H

#include<stdbool.h>
#include "core.h"

typedef struct Launcher* Launcher_t;

typedef enum {
  LAUNCHER_DOWN,
  LAUNCHER_UP,
  LAUNCHER_LEFT,
  LAUNCHER_RIGHT,
  LAUNCHER_FIRE,
  LAUNCHER_STOP,
  LAUNCHER_LEDON,
  LAUNCHER_LEDOFF,
} LauncherCmd;

Launcher_t launcherStart();
void launcherStop(Launcher_t c);
bool launcherSend(Launcher_t launcher, LauncherCmd c);

#endif //THUNDER_LAUNCHER_H
