#include "FF.h"
#include <iostream>

FF::FF(int x, int y, std::string inst_name, LibCell* lib_cell, std::pair<Pin*, Pin*> dqpair, Pin* clk)
{
    if (lib_cell->bit > 1)
    {
        std::cerr << "Error: base FF cell bit > 1" << std::endl;
        exit(1);
    }
    _x = x;
    _y = y;
    _inst_name = inst_name;
    _lib_cell = lib_cell;

    // clk
    _clkPin = new Pin(*(lib_cell->clkPin));
    _clkPin->setCell(this);
    _clkPin->setOriginalName(clk->getOriginalName());
    _clkPin->copyConnection(clk);
    _pins.push_back(_clkPin);

    // d
    Pin* newInPin = new Pin(*(lib_cell->inputPins[0]));
    newInPin->setCell(this);
    newInPin->setOriginalName(dqpair.first->getOriginalName());
    newInPin->copyConnection(dqpair.first);
    this->_inputPins.push_back(newInPin);
    this->_pins.push_back(newInPin);

    // q
    Pin* newOutPin = new Pin(*(lib_cell->outputPins[0]));
    newOutPin->setCell(this);
    newOutPin->setOriginalName(dqpair.second->getOriginalName());
    newOutPin->copyConnection(dqpair.second);
    this->_outputPins.push_back(newOutPin);
    this->_pins.push_back(newOutPin);
}

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

Pin* FF::getClkPin()
{
    return _clkPin;
}

std::vector<std::pair<Pin*, Pin*>> FF::getDQpairs()
{
    std::vector<std::pair<Pin*, Pin*>> dqPairs;
    const int bit = this->getBit();
    for (int i = 0; i < bit; i++)
    {
        dqPairs.push_back(std::make_pair(this->_inputPins[i], this->_outputPins[i]));
    }
    return dqPairs;
}
