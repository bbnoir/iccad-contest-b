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
    _clkPin = clk;
    _clkPin->transInfo(lib_cell->clkPin);

    // d
    Pin* newInPin = dqpair.first;
    newInPin->transInfo(lib_cell->inputPins[0]);
    this->_inputPins.push_back(newInPin);

    // q
    Pin* newOutPin = dqpair.second;
    newOutPin->transInfo(lib_cell->outputPins[0]);
    this->_outputPins.push_back(newOutPin);
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
