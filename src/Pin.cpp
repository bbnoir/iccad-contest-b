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

std::string Pin::getOriginalName()
{
    return _originalCellPinName;
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
    if (_cell != nullptr)
    {
        return _cell->getX() + _x;
    }
    return _x;
}

int Pin::getGlobalY()
{
    if (_cell != nullptr)
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

Net* Pin::getNet()
{
    return _net;
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

void Pin::setOriginalName()
{
    _originalCellPinName = this->getCell()->getInstName() + "/" + this->getName();
}

void Pin::setOriginalName(std::string ori_name)
{
    _originalCellPinName = ori_name;
}

void Pin::setFaninPin(Pin* pin)
{
    _faninPin = pin;
}

void Pin::addFanoutPin(Pin* pin)
{
    _fanoutPins.push_back(pin);
}

void Pin::connect(Net* net)
{
    _net = net;
}

Pin* Pin::getFaninPin()
{
    return _faninPin;
}

Pin* Pin::getFirstFanoutPin()
{
    if (_fanoutPins.size() > 0)
    {
        return _fanoutPins[0];
    }
    return nullptr;
}

std::vector<Pin*> Pin::getFanoutPins()
{
    return _fanoutPins;
}

void Pin::copyConnection(Pin* pin)
{
    _net = pin->getNet();
    _faninPin = pin->getFaninPin();
    _fanoutPins = pin->getFanoutPins();
}

void Pin::transInfo(Pin* pin)
{
    _x = pin->getX();
    _y = pin->getY();
    _name = pin->getName();
    _isDpin = pin->isDpin();
    _type = pin->getType();
}