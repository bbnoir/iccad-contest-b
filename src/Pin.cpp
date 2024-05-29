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

void Pin::connect(Net* net)
{
    _net = net;
}