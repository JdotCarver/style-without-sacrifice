#pragma once
#include "ControllerState.hpp"
namespace Wardrobe {
void startControllerInput();void stopControllerInput();void resetControllerInput();
ControllerSample controllerSample();
bool menuControllerFocused();
uint64_t menuInputQueries();
const wchar_t* menuInputStatus();
}
