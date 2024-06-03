#include "Net.h"

Net::Net(std::string name)
{
    this->name = name;
}

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

void Net::setPins(std::vector<Pin*> pins)
{
    this->pins = pins;
}