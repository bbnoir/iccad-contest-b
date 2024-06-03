#include "Cell.h"

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
    // copy pins
    for(auto pin : lib_cell->pins)
    {
        pins.push_back(new Pin(pin));
    }
    for (auto pin : pins)
    {
        pin->setCell(this);
    }
}

Cell::~Cell()
{
}

int Cell::getX()
{
    return _x;
}

int Cell::getY()
{
    return _y;
}

int Cell::getArea()
{
    return _lib_cell->width * _lib_cell->height;
}

double Cell::getPower()
{
    return _lib_cell->power;
}

std::string Cell::getInstName()
{
    return _inst_name;
}

std::string Cell::getCellName()
{
    return _lib_cell->cell_name;
}

int Cell::getWidth()
{
    return _lib_cell->width;
}

int Cell::getHeight()
{
    return _lib_cell->height;
}

std::vector<Pin*> Cell::getPins()
{
    return pins;
}

Pin* Cell::getPin(std::string pin_name)
{
    for(auto pin : pins)
    {
        if(pin->getName() == pin_name)
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

void Cell::setX(int x)
{
    this->_x = x;
}

void Cell::setY(int y)
{
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
    pins.push_back(pin);
}