#include "Cell.h"
#include <iostream>

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

    for (Pin* pin : lib_cell->inputPins)
    {
        Pin* newPin = new Pin(*pin);
        newPin->setCell(this);
        newPin->setOriginalName();
        newPin->copyConnection(pin);
        this->_inputPins.push_back(newPin);
        this->_pins.push_back(newPin);
    }
    for (Pin* pin : lib_cell->outputPins)
    {
        Pin* newPin = new Pin(*pin);
        newPin->setCell(this);
        newPin->setOriginalName();
        newPin->copyConnection(pin);
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

int Cell::getArea()
{
    return _lib_cell->width * _lib_cell->height;
}

std::string Cell::getInstName()
{
    return _inst_name;
}

std::string Cell::getCellName()
{
    return _lib_cell->cell_name;
}

CellType Cell::getCellType()
{
    return _lib_cell->type;
}

std::vector<Pin*> Cell::getPins()
{
    return _pins;
}

std::vector<Pin*> Cell::getInputPins()
{
    return _inputPins;
}

std::vector<Pin*> Cell::getOutputPins()
{
    return _outputPins;
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

std::vector<Site*> Cell::getSites()
{
    return _sites;
}

std::vector<Bin*> Cell::getBins()
{
    return _bins;
}

LibCell* Cell::getLibCell()
{
    return _lib_cell;
}

void Cell::setX(int x)
{
    this->_x = x;
}

void Cell::setY(int y)
{
    this->_y = y;
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

void Cell::addSite(Site* site)
{
    this->_sites.push_back(site);
}

void Cell::addBin(Bin* bin)
{
    this->_bins.push_back(bin);
}

void Cell::addPin(Pin* pin)
{
    _pins.push_back(pin);
}

void Cell::removeSite(Site* site)
{
    auto it = std::find(_sites.begin(), _sites.end(), site);
    if(it != _sites.end())
    {
        _sites.erase(it);
    }
}

void Cell::removeBin(Bin* bin)
{
    auto it = std::find(_bins.begin(), _bins.end(), bin);
    if(it != _bins.end())
    {
        _bins.erase(it);
    }
}


/*
Check overlap with other cells in the same bin
*/
bool Cell::checkOverlap()
{
    for(auto bin: _bins){
        for(auto cell: bin->getCells()){
            if(cell == this){
                continue;
            }
            if((this->_x < cell->getX() + cell->getWidth()) && (this->_x + this->getWidth() > cell->getX()) &&
               (this->_y < cell->getY() + cell->getHeight()) && (this->_y + this->getHeight() > cell->getY())){
                return true;
            }
        }
    }
    return false;
}

double Cell::getQDelay()
{
    return _lib_cell->qDelay;
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
