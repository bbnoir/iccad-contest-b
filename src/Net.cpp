#include "Net.h"

Net::Net(std::string name, std::vector<Pin*> pins)
{
    this->name = name;
    this->pins = pins;
}

Net::~Net()
{
}

std::string Net::getName()
{
    return name;
}

std::vector<Pin*> Net::getPins()
{
    return pins;
}