#include "Cell.h"

Cell::Cell()
{
    x = 0;
    y = 0;
}

Cell::Cell(int x, int y)
{
    this->x = x;
    this->y = y;
}

Cell::~Cell()
{
}

int Cell::getX()
{
    return x;
}

int Cell::getY()
{
    return y;
}

void Cell::setX(int x)
{
    this->x = x;
}

void Cell::setY(int y)
{
    this->y = y;
}