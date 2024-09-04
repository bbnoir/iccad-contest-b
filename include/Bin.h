#pragma once
#include <vector>
#include <algorithm>
#include "param.h"

class Cell;
class FF;
struct LibCell;

class Bin
{
    public:
        Bin();
        Bin(int x, int y);
        ~Bin();

        inline int getX() const { return _x; }
        inline int getY() const { return _y; }
        inline double getUtilization() const { return _utilization; }
        inline bool isOverMaxUtil() const { return _utilization >= BIN_MAX_UTIL; }
        inline const std::vector<Cell*>& getCells() const { return _cells; }

        double addCell(Cell* cell, bool trial = false);
        double removeCell(Cell* cell, bool trial = false);

        int calOverlapArea(Cell* cell);
    private:
        int _x;
        int _y;
        double _utilization;
        std::vector<Cell*> _cells;

};

class BinMap
{
    public:
        BinMap();
        BinMap(int dieLowerLeftX, int dieLowerLeftY, int dieUpperRightX, int dieUpperRightY, int binWidth, int binHeight);

        std::vector<Bin*> getBins(int leftDownX, int leftDownY, int rightUpX, int rightUpY);
        std::vector<Bin*> getBins();

        int getNumOverMaxUtilBins();

        double trialLibCell(LibCell* libCell, int x, int y);
        double addCell(Cell* cell, bool trial = false);
        double addCell(Cell* cell, int x, int y, bool trial = false);
        double removeCell(Cell* cell, bool trial = false);
        double moveCell(Cell* cell, int x, int y, bool trial = false);

    private:
        int _dieLowerLeftX;
        int _dieLowerLeftY;
        int _dieUpperRightX;
        int _dieUpperRightY;
        int _binWidth;
        int _binHeight;
        std::vector<std::vector<Bin*>> _bins;
};