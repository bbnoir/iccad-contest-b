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
    // update utilization
    // calculate the overlap area
    int overlapArea = _calOverlapArea(cell);
    _utilization += (double)overlapArea / (BIN_WIDTH * BIN_HEIGHT);
}

void Bin::removeCell(Cell* cell)
{
    if (std::find(_cells.begin(), _cells.end(), cell) != _cells.end())
    {
        _cells.erase(std::remove(_cells.begin(), _cells.end(), cell), _cells.end());
    }
    // update utilization
    // calculate the overlap area
    int overlapArea = _calOverlapArea(cell);
    _utilization -= (double)overlapArea / (BIN_WIDTH * BIN_HEIGHT);
}

int Bin::_calOverlapArea(Cell* cell)
{
    int x1 = _x;
    int y1 = _y;
    int w1 = BIN_WIDTH;
    int h1 = BIN_HEIGHT;
    int x2 = cell->getX();
    int y2 = cell->getY();
    int w2 = cell->getWidth();
    int h2 = cell->getHeight();
    int x_overlap = std::max(0, std::min(x1 + w1, x2 + w2) - std::max(x1, x2));
    int y_overlap = std::max(0, std::min(y1 + h1, y2 + h2) - std::max(y1, y2));
    return x_overlap * y_overlap;
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

void BinMap::addCell(Cell* cell)
{
    int leftDownX = cell->getX();
    int leftDownY = cell->getY();
    int rightUpX = cell->getX() + cell->getWidth();
    int rightUpY = cell->getY() + cell->getHeight();
    std::vector<Bin*> bins = getBins(leftDownX, leftDownY, rightUpX, rightUpY);
    for (auto bin : bins)
    {
        bin->addCell(cell);
        cell->addBin(bin);
    }
}

void BinMap::removeCell(Cell* cell)
{
    int leftDownX = cell->getX();
    int leftDownY = cell->getY();
    int rightUpX = cell->getX() + cell->getWidth();
    int rightUpY = cell->getY() + cell->getHeight();
    std::vector<Bin*> bins = getBins(leftDownX, leftDownY, rightUpX, rightUpY);
    for (auto bin : bins)
    {
        bin->removeCell(cell);
        cell->removeBin(bin);
    }
}

void BinMap::moveCell(Cell* cell, int x, int y)
{
    removeCell(cell);
    cell->setX(x);
    cell->setY(y);
    addCell(cell);
}

void BinMap::moveCell(Cell* cell)
{
    removeCell(cell);
    addCell(cell);
}