#pragma once
#include <vector>
#include <algorithm>

class Cell;

class Bin
{
    public:
        Bin();
        Bin(int x, int y);
        ~Bin();

        int getX();
        int getY();
        double getUtilization();
        bool isOverMaxUtil();
        const std::vector<Cell*>& getCells();

        void addCell(Cell* cell);
        void removeCell(Cell* cell);

    private:
        int _x;
        int _y;
        double _utilization;
        std::vector<Cell*> _cells;

        int _calOverlapArea(Cell* cell);
};

class BinMap
{
    public:
        BinMap();
        BinMap(int dieLowerLeftX, int dieLowerLeftY, int dieUpperRightX, int dieUpperRightY, int binWidth, int binHeight);

        std::vector<Bin*> getBins();
        std::vector<Bin*> getBins(int leftDownX, int leftDownY, int rightUpX, int rightUpY);

        void addCell(Cell* cell);
        void removeCell(Cell* cell);

    private:
        int _dieLowerLeftX;
        int _dieLowerLeftY;
        int _dieUpperRightX;
        int _dieUpperRightY;
        int _binWidth;
        int _binHeight;
        int _numBinsX;
        int _numBinsY;
        std::vector<std::vector<Bin*>> _bins;
};