#include "Site.h"

Site::Site()
{
    _x = 0;
    _y = 0;
    _width = 0;
    _height = 0;
}

Site::Site(int x, int y, int width, int height)
{
    this->_x = x;
    this->_y = y;
    this->_width = width;
    this->_height = height;
}

Site::~Site()
{
}

void Site::place(Cell* cell)
{
    this->_cell = cell;
}