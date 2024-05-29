#include "Comb.h"

Comb::~Comb()
{
}

void Comb::addPin(Pin* pin)
{
    pins.push_back(pin);
}