#include "Bin.h"
#include "Cell.h"
#include "param.h"
#include <cmath>
#include <iostream>

Bin::Bin()
{
}

Bin::Bin(int x, int y)
{
    _x = x;
    _y = y;
    _utilization = 0;
}

Bin::~Bin()
{
}

int Bin::getX()
{
    return _x;
}

int Bin::getY()
{
    return _y;
}

double Bin::getUtilization()
{
    return _utilization;
}

bool Bin::isOverMaxUtil()
{
    return _utilization >= BIN_MAX_UTIL;
}

const std::vector<Cell*>& Bin::getCells()
{
    return _cells;
}

void Bin::addCell(Cell* cell)
{
    _cells.push_back(cell);
    // TODO: update utilization
    _utilization += (double)cell->getArea() / (BIN_WIDTH * BIN_HEIGHT);
}

void Bin::removeCell(Cell* cell)
{
    if (std::find(_cells.begin(), _cells.end(), cell) != _cells.end())
    {
        _cells.erase(std::remove(_cells.begin(), _cells.end(), cell), _cells.end());
    }
    // TODO: update utilization
    _utilization -= (double)cell->getArea() / (BIN_WIDTH * BIN_HEIGHT);
}

BinMap::BinMap()
{
}

BinMap::BinMap(int dieLowerLeftX, int dieLowerLeftY, int dieUpperRightX, int dieUpperRightY, int binWidth, int binHeight)
{
    _dieLowerLeftX = dieLowerLeftX;
    _dieLowerLeftY = dieLowerLeftY;
    _dieUpperRightX = dieUpperRightX;
    _dieUpperRightY = dieUpperRightY;
    _binWidth = binWidth;
    _binHeight = binHeight;
    for (int y = dieLowerLeftY; y < dieUpperRightY; y += binHeight)
    {
        std::vector<Bin*> row;
        for (int x = dieLowerLeftX; x < dieUpperRightX; x += binWidth)
        {
            row.emplace_back(new Bin(x, y));
        }
        _bins.emplace_back(row);
    }
}

std::vector<Bin*> BinMap::getBins()
{
    std::vector<Bin*> bins;
    for (auto row : _bins)
    {
        for (auto bin : row)
        {
            bins.emplace_back(bin);
        }
    }
    return bins;
}

std::vector<Bin*> BinMap::getBins(int leftDownX, int leftDownY, int rightUpX, int rightUpY)
{
    if (leftDownX < _dieLowerLeftX || leftDownY < _dieLowerLeftY || rightUpX > _dieUpperRightX || rightUpY > _dieUpperRightY)
    {
        std::cerr << "Error: getBins out of range" << std::endl;
        exit(1);
    }
    std::vector<Bin*> bins;
    const int startBinX = (leftDownX - _dieLowerLeftX) / _binWidth;
    const int startBinY = (leftDownY - _dieLowerLeftY) / _binHeight;
    const int endBinX = std::ceil((double)(rightUpX - _dieLowerLeftX) / _binWidth);
    const int endBinY = std::ceil((double)(rightUpY - _dieLowerLeftY) / _binHeight);
    for (int i = startBinY; i < endBinY; i++)
    {
        for (int j = startBinX; j < endBinX; j++)
        {
            bins.emplace_back(_bins[i][j]);
        }
    }
    return bins;
}