#pragma once
#include <vector>
#include "Cell.h"
#include "Bin.h"

class Cell;
class Bin;

class Site
{
    public:
        Site();
        Site(int x, int y, int width, int height);
        ~Site();

        void place(Cell* cell);

    private:
        int _x;
        int _y;
        int _width;
        int _height;
        Cell* _cell;
        // affected bins
        std::vector<Bin*> _bins;
};