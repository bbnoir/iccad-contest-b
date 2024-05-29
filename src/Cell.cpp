#include "Cell.h"

Cell::Cell()
{
    _x = 0;
    _y = 0;
    _site = nullptr;
    _area = 0;
}

Cell::Cell(int x, int y)
{
    this->_x = x;
    this->_y = y;
    this->_site = nullptr;
    this->_area = 0;
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

void Cell::setX(int x)
{
    this->_x = x;
}

void Cell::setY(int y)
{
    this->_y = y;
}

void Cell::setSite(Site* site)
{
    this->_site = site;
}