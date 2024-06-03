#include "Pin.h"

Pin::Pin(PinType type, int x, int y, std::string name, Cell* cell)
{
    _type = type;
    _x = x;
    _y = y;
    _name = name;
    _cell = cell;
    _slack = 0;
    _isDpin = false;
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

int Pin::getGlobalX()
{
    if(_type == PinType::CELL)
    {
        return _cell->getX() + _x;
    }
    return _x;
}

int Pin::getGlobalY()
{
    if(_type == PinType::CELL)
    {
        return _cell->getY() + _y;
    }
    return _y;
}

bool Pin::isDpin()
{
    return _isDpin;
}

double Pin::getSlack()
{
    return _slack;
}

Cell* Pin::getCell()
{
    return _cell;
}

PinType Pin::getType()
{
    return _type;
}

void Pin::setSlack(double slack)
{
    _slack = slack;
    _isDpin = true;
}

void Pin::setCell(Cell* cell)
{
    _cell = cell;
}

void Pin::connect(Net* net)
{
    _net = net;
}