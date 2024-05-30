#include "Pin.h"

Pin::Pin(PinType type, int x, int y, std::string name, Cell* cell)
{
    _type = type;
    _x = x;
    _y = y;
    _name = name;
    _cell = cell;
}

Pin::~Pin()
{
}

std::string Pin::getName()
{
    return _name;
}

int Pin::getX()
{
    return _x;
}

int Pin::getY()
{
    return _y;
}

void Pin::connect(Net* net)
{
    _net = net;
}