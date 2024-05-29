#pragma once
#include "Site.h"

class Cell
{
public:
    Cell();
    Cell(int x, int y);
    ~Cell();

    int getX();
    int getY();
    int getArea();

    void setX(int x);
    void setY(int y);

    void setSite(Site* site);

protected:
    int _x;
    int _y;
    int _area;
    // Site where the cell is placed
    Site* _site;
};