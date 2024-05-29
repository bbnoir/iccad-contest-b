#pragma once
#include <vector>
#include "Cell.h"

class Bin
{
    public:
        Bin();
        ~Bin();

    private:
        int _x;
        int _y;
        // std::vector<Cell*> _cells;
};