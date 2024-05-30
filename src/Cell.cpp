#include "Cell.h"

Cell::Cell()
{
    _x = 0;
    _y = 0;
    _site = nullptr;
    _area = 0;
    _type = CellType::INST;
}

Cell::Cell(int width, int height, std::string cell_name)
{
    _x = 0;
    _y = 0;
    _width = width;
    _height = height;
    _cell_name = cell_name;
    _site = nullptr;
    _area = width * height;
    _type = CellType::LIB;
}

Cell::Cell(int x, int y, int width, int height, std::string inst_name, std::string cell_name)
{
    _x = x;
    _y = y;
    _width = width;
    _height = height;
    _inst_name = inst_name;
    _cell_name = cell_name;
    _site = nullptr;
    _area = width * height;
    _type = CellType::INST;
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
    return _area;
}

std::string Cell::getInstName()
{
    return _inst_name;
}

std::string Cell::getCellName()
{
    return _cell_name;
}

int Cell::getWidth()
{
    return _width;
}

int Cell::getHeight()
{
    return _height;
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

void Cell::setX(int x)
{
    this->_x = x;
}

void Cell::setY(int y)
{
    this->_y = y;
}

void Cell::setCellName(std::string cell_name)
{
    this->_cell_name = cell_name;
}

void Cell::setInstName(std::string inst_name)
{
    this->_inst_name = inst_name;
}

void Cell::setSite(Site* site)
{
    this->_site = site;
}

void Cell::addPin(Pin* pin)
{
    pins.push_back(pin);
}