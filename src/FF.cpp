#include "FF.h"

FF::~FF()
{
}

int FF::getBit()
{
    return _lib_cell->bit;
}

double FF::getQDelay()
{
    return _lib_cell->qDelay;
}