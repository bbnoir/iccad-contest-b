#include "Cell.h"
#include <iostream>

LibCell::~LibCell()
{
    for (auto pin : inputPins)
    {
        if (pin != nullptr)
        {
            delete pin;
        }
    }
    for (auto pin : outputPins)
    {
        if (pin != nullptr)
        {
            delete pin;
        }
    }
    if (clkPin != nullptr)
    {
        delete clkPin;
    }
}

Cell::Cell()
{
    _x = 0;
    _y = 0;
    _inst_name = "";
}

Cell::Cell(int x, int y, std::string inst_name, LibCell* lib_cell)
{
    _x = x;
    _y = y;
    _inst_name = inst_name;
    _lib_cell = lib_cell;
    // copy pins and set cell

    if(inst_name == "du_mb"){
        return;
    }

    for (Pin* pin : lib_cell->inputPins)
    {
        Pin* newPin = new Pin(*pin);
        newPin->setCell(this);
        newPin->setOriginalName();
        this->_inputPins.push_back(newPin);
        this->_pins.push_back(newPin);
    }
    for (Pin* pin : lib_cell->outputPins)
    {
        Pin* newPin = new Pin(*pin);
        newPin->setCell(this);
        newPin->setOriginalName();
        this->_outputPins.push_back(newPin);
        this->_pins.push_back(newPin);
    }
    if (lib_cell->clkPin != nullptr)
    {
        Pin* newPin = new Pin(*(lib_cell->clkPin));
        newPin->setCell(this);
        newPin->setOriginalName();
        this->_clkPin = newPin;
        this->_pins.push_back(newPin);
    }
}

Cell::~Cell()
{
}

Pin* Cell::getPin(std::string pin_name)
{
    for (auto pin : _pins)
    {
        if (pin->getName() == pin_name)
        {
            return pin;
        }
    }
    return nullptr;
}

void Cell::setXY(int x, int y)
{
    this->_x = x;
    this->_y = y;
}

void Cell::setInstName(std::string inst_name)
{
    this->_inst_name = inst_name;
}

void Cell::addPin(Pin* pin)
{
    _pins.push_back(pin);
}

void Cell::deletePins()
{
    for (auto pin : _pins)
    {
        delete pin;
    }
    _pins.clear();
    _inputPins.clear();
    _outputPins.clear();
}
