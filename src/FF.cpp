#include "FF.h"
#include <iostream>
/*
Constructor for single bits
*/
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
    Pin* libClkPin = lib_cell->clkPin;
    _clkPin = new Pin(PinType::FF_CLK, libClkPin->getX(), libClkPin->getY(), libClkPin->getName(), this);
    _clkPin->copyConnection(clk);
    _clkPin->setOriginalName(clk->getOriginalName());

    // d
    Pin* newInPin = dqpair.first;
    newInPin->transInfo(lib_cell->inputPins[0]);
    this->_inputPins.push_back(newInPin);
    newInPin->setCell(this);

    // q
    Pin* newOutPin = dqpair.second;
    newOutPin->transInfo(lib_cell->outputPins[0]);
    this->_outputPins.push_back(newOutPin);
    newOutPin->setCell(this);
}

FF::FF(int x, int y, std::string inst_name, LibCell* lib_cell, std::vector<std::pair<Pin*, Pin*>> dqpairs, Pin* clk)
{
    _x = x;
    _y = y;
    _inst_name = inst_name;
    _lib_cell = lib_cell;

    // clk
    Pin* libClkPin = lib_cell->clkPin;
    _clkPin = new Pin(PinType::FF_CLK, libClkPin->getX(), libClkPin->getY(), libClkPin->getName(), this);
    _clkPin->copyConnection(clk);
    _clkPin->setOriginalName(clk->getOriginalName());

    for(size_t i=0;i<dqpairs.size();i++)
    {
        // d
        Pin* newInPin = dqpairs[i].first;
        newInPin->transInfo(lib_cell->inputPins[i]);
        this->_inputPins.push_back(newInPin);
        newInPin->setCell(this);

        // q
        Pin* newOutPin = dqpairs[i].second;
        newOutPin->transInfo(lib_cell->outputPins[i]);
        this->_outputPins.push_back(newOutPin);
        newOutPin->setCell(this);
    }
}

/*
Constructor for banking
*/
FF::FF(int x, int y, std::string inst_name, LibCell* lib_cell, std::vector<std::pair<Pin*, Pin*>> dqpairs, std::vector<Pin*> clks)
{
    if (lib_cell->bit < int(dqpairs.size()))
    {
        std::cerr << "Error: lib FF cell bit < dqpairs.size()" << std::endl;
        exit(1);
    }
    _x = x;
    _y = y;
    _inst_name = inst_name;
    _lib_cell = lib_cell;

    // clk
    Pin* libClkPin = lib_cell->clkPin;
    _clkPin = new Pin(PinType::FF_CLK, libClkPin->getX(), libClkPin->getY(), libClkPin->getName(), this);
    _clkPin->copyConnection(clks[0]);
    for (auto clk : clks)
    {
        std::vector<std::string> ori_names = clk->getOriginalNames();
        for (auto ori_name : ori_names)
        {
            _clkPin->addOriginalName(ori_name);
        }
    }

    // dq
    for (size_t i = 0; i < dqpairs.size(); i++)
    {
        Pin* newInPin = dqpairs[i].first;
        newInPin->transInfo(lib_cell->inputPins[i]);
        this->_inputPins.push_back(newInPin);
        newInPin->setCell(this);

        Pin* newOutPin = dqpairs[i].second;
        newOutPin->transInfo(lib_cell->outputPins[i]);
        this->_outputPins.push_back(newOutPin);
        newOutPin->setCell(this);
    }

    for(auto clk : clks)
    {
        delete clk;
    }
}

FF::~FF()
{
}

double FF::getTotalNegativeSlack()
{
    double totalNegativeSlack = 0.0;
    for (Pin* pin : _inputPins)
    {
        double slack = pin->getSlack();
        if (slack < 0)
        {
            totalNegativeSlack += slack;
        }
    }
    return -totalNegativeSlack;
}

int FF::getNSPinCount()
{
    int count = 0;
    for (auto inPin : _inputPins)
    {
        if (inPin->getSlack() < 0)
        {
            count++;
        }
    }
    for (auto outPin : _outputPins)
    {
        for (auto nextPin : outPin->getNextStagePins())
        {
            if (nextPin->getSlack() < 0)
            {
                count++;
            }
        }
    }
    return count;
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

void FF::setClkDomain(int clkDomain)
{
    this->clkDomain = clkDomain;
}