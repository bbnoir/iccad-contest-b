#include "FF.h"

FF::~FF()
{
}

void FF::addPin(Pin* pin)
{
    pins.push_back(pin);
}