#pragma once
#include <vector>
#include "Cell.h"

class Cell;

class Bin
{
    public:
        Bin();
        ~Bin();

    private:
        int _x;
        int _y;
        std::vector<Cell*> _cells;
};