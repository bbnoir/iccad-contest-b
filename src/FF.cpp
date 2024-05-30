#include "FF.h"

FF::~FF()
{
}

int FF::getBit()
{
    return this->_bit;
}

double FF::getQDelay()
{
    return this->_qDelay;
}

double FF::getPower()
{
    return this->_power;
}

void FF::setBit(int bit)
{
    this->_bit = bit;
}

void FF::setQDelay(double qDelay)
{
    this->_qDelay = qDelay;
}

void FF::setPower(double power)
{
    this->_power = power;
}