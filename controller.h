#ifndef THUNDER_CONTROLLER_H
#define THUNDER_CONTROLLER_H

#include "core.h"

typedef struct Controller* Controller_t;

Controller_t controllerInit(Core_t core);
void controllerStart(Controller_t c);
void controllerStop(Controller_t c);

#endif //THUNDER_CONTROLLER_H
