#pragma once
#include <vector>
#include <algorithm>

class Cell;
class FF;

class Bin
{
    public:
        Bin();
        Bin(int x, int y);
        ~Bin();

        int getX() const { return _x; }
        int getY() const { return _y; }
        double getUtilization();
        bool isOverMaxUtil();
        const std::vector<Cell*>& getCells();

        double addCell(Cell* cell, bool trial = false);
        double removeCell(Cell* cell, bool trial = false);

        bool totallyContains(Cell* cell);


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

        std::vector<Bin*> getBins();
        std::vector<std::vector<Bin*>> getBins2D();
        std::vector<Bin*> getBins(int leftDownX, int leftDownY, int rightUpX, int rightUpY);
        std::vector<Bin*> getBinsBlocks(int indexX, int indexY, int numBlocksX, int numBlocksY);
        std::vector<FF*> getFFsInBins(const std::vector<Bin*>& bins);
        std::vector<FF*> getFFsInBinsBlocks(int indexX, int indexY, int numBlocksX, int numBlocksY);

        int getNumBinsX();
        int getNumBinsY();

        int getNumOverMaxUtilBins();
        int getNumOverMaxUtilBinsByComb();

        double addCell(Cell* cell, bool trial = false);
        double removeCell(Cell* cell, bool trial = false);

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