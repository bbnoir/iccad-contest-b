#include <queue>
#include <utility>
#include "Site.h"
#include "param.h"
#include "Solver.h"
#include "Cell.h"
#include "Bin.h"

Site::Site()
{
    _x = 0;
    _y = 0;
    _width = 0;
    _height = 0;
}

Site::Site(int x, int y, int width, int height)
{
    this->_x = x;
    this->_y = y;
    this->_width = width;
    this->_height = height;
}

Site::~Site()
{
}

void Site::place(Cell* cell)
{
    this->_cells.push_back(cell);
}

void Site::removeCell(Cell* cell)
{
    if (std::find(_cells.begin(), _cells.end(), cell) != _cells.end())
    {
        _cells.erase(std::remove(_cells.begin(), _cells.end(), cell), _cells.end());
    }
}

SiteMap::SiteMap()
{
}

SiteMap::SiteMap(std::vector<PlacementRows> placementRows)
{
    _hasMultiPlaceRow = false;
    _placementRows = placementRows;
    _sites.resize(placementRows.size());
    _x2col.resize(placementRows.size());
    const int nPlacementRows = placementRows.size();
    for (int i = 0; i < nPlacementRows; i++)
    {
        PlacementRows& row = placementRows[i];
        int siteX = row.startX, siteY = row.startY;
        // set y2row to the first row with the same y
        if (_y2row.find(siteY) == _y2row.end())
        {
            _y2row[siteY] = i;
        }
        else
        {
            _hasMultiPlaceRow = true;
        }
        for (int j = 0; j < row.numSites; j++)
        {
            // prune the sites that are out of the die boundary
            if (siteX < DIE_LOW_LEFT_X)
            {
                siteX += row.siteWidth;
                continue;
            }
            else if (siteX > DIE_UP_RIGHT_X)
            {
                row.numSites = j;
                break;
            }
            Site* site = new Site(siteX, siteY, row.siteWidth, row.siteHeight);
            _x2col[i][siteX] = j;
            _sites[i].push_back(site);
            siteX += row.siteWidth;
        }
    }
}

std::vector<Site*> SiteMap::getSites()
{
    std::vector<Site*> sites;
    for (const auto& row : _sites)
    {
        for (Site* site : row)
        {
            sites.push_back(site);
        }
    }
    return sites;
}

std::vector<Site*> SiteMap::getSitesOfCell(int leftDownX, int leftDownY, int rightUpX, int rightUpY)
{
    if (!onSite(leftDownX, leftDownY) || rightUpX > DIE_UP_RIGHT_X || rightUpY > DIE_UP_RIGHT_Y)
    {
        return std::vector<Site*>();    
    }
    std::vector<Site*> sites;
    const int startRow = _y2row[leftDownY];
    const int endRow = getFirstLargerRow(rightUpY);
    for (int row = startRow; row < endRow; row++)
    {
        if (leftDownX > _placementRows[row].startX + _placementRows[row].siteWidth * _placementRows[row].numSites || rightUpX < _placementRows[row].startX)
        {
            continue;
        }
        const int startCol = std::max(0, (leftDownX - _placementRows[row].startX) / _placementRows[row].siteWidth);
        int endCol = getFirstLargerColInRow(row, rightUpX);
        for (int col = startCol; col <= endCol; col++)
        {
            sites.push_back(_sites[row][col]);
        }
    }
    return sites;
}

/*
Get all sites in the block(fully covered by the block)
*/
std::vector<Site*> SiteMap::getSitesInBlock(int leftDownX, int leftDownY, int rightUpX, int rightUpY)
{
    if (leftDownX < DIE_LOW_LEFT_X || leftDownY < DIE_LOW_LEFT_Y || rightUpX > DIE_UP_RIGHT_X || rightUpY > DIE_UP_RIGHT_Y)
    {
        std::cerr << "Error: SiteMap::getSitesInBlock() - out of die boundary" << std::endl;
        return std::vector<Site*>();
    }
    std::vector<Site*> sites;
    const int startRow = getFirstLargerRow(leftDownY);
    const int endRow = getFirstLargerRow(rightUpY);
    for (int row = startRow; row < endRow; row++)
    {
        const int startCol = getFirstLargerColInRow(row, leftDownX);
        int endCol = getFirstLargerColInRow(row, rightUpX);
        for (int col = startCol; col <= endCol; col++)
        {
            sites.push_back(_sites[row][col]);
        }
    }
    return sites;
}

int SiteMap::getFirstLargerRow(int y)
{
    if (y < DIE_LOW_LEFT_Y || y > DIE_UP_RIGHT_Y)
    {
        std::cerr << "Error: SiteMap::getFirstLargerRow() - out of die boundary" << std::endl;
        return _placementRows.size()-1;
    }
    int row = _placementRows.size()-1;
    // binary search
    int left = 0;
    int right = row;
    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        if (_placementRows[mid].startY < y)
        {
            left = mid + 1;
        }
        else
        {
            row = mid;
            right = mid - 1;
        }
    }
    return row;
}

int SiteMap::getFirstLargerColInRow(int row, int x)
{
    if (x < DIE_LOW_LEFT_X || x > DIE_UP_RIGHT_X)
    {
        std::cerr << "Error: SiteMap::getFirstLargerColInRow() - out of die boundary" << std::endl;
        return _placementRows[row].numSites-1;
    }
    int col = (x - _placementRows[row].startX) / _placementRows[row].siteWidth + 1;
    col = std::max(0, col);
    col = std::min(col, _placementRows[row].numSites - 1);
    return col;
}

Site* SiteMap::getNearestSite(int x, int y)
{
    if (x < DIE_LOW_LEFT_X || y < DIE_LOW_LEFT_Y || x > DIE_UP_RIGHT_X || y > DIE_UP_RIGHT_Y)
    {
        std::cerr << "Error: SiteMap::getNearestSite() - out of die boundary" << std::endl;
        return nullptr;
    }
    int distance;
    int minDistance = -1;
    Site* nearestSite = nullptr;
    //TODO: rows with same y
    int topRow = getFirstLargerRow(y);
    int botRow = (topRow - 1)>=0?topRow - 1:0;
    if (_hasMultiPlaceRow)
    {
        while (x > _placementRows[topRow].startX + _placementRows[topRow].siteWidth * _placementRows[topRow].numSites && topRow+1 < int(_placementRows.size()) && _placementRows[topRow].startY == _placementRows[topRow+1].startY)
        {
            topRow++;
        }
        while (x < _placementRows[botRow].startX && botRow-1 >= 0 && _placementRows[botRow].startY == _placementRows[botRow-1].startY)
        {
            botRow--;
        }
    }
    int toprightCol = getFirstLargerColInRow(topRow, x);
    int topleftCol = toprightCol - 1;
    int botrightCol = getFirstLargerColInRow(botRow, x);
    int botleftCol = botrightCol - 1;

    // top right
    if (topRow < int(_placementRows.size()))
    {
        distance = abs(_placementRows[topRow].startY - y) + abs(_placementRows[topRow].startX + _placementRows[topRow].siteWidth*toprightCol - x);
        if (minDistance == -1 || distance < minDistance)
        {
            minDistance = distance;
            nearestSite = _sites[topRow][toprightCol];
        }
    }
    // top left
    if (topRow < int(_placementRows.size()) && topleftCol >=0 )
    {
        distance = abs(_placementRows[topRow].startY - y) + abs(_placementRows[topRow].startX + _placementRows[topRow].siteWidth*topleftCol - x);
        if (minDistance == -1 || distance < minDistance)
        {
            minDistance = distance;
            nearestSite = _sites[topRow][topleftCol];
        }
    }
    // bot right
    if (botRow >= 0)
    {
        distance = abs(_placementRows[botRow].startY - y) + abs(_placementRows[botRow].startX + _placementRows[botRow].siteWidth*botrightCol - x);
        if (minDistance == -1 || distance < minDistance)
        {
            minDistance = distance;
            nearestSite = _sites[botRow][botrightCol];
        }
    }
    // bot left
    if (botRow >= 0 && botleftCol >= 0)
    {
        distance = abs(_placementRows[botRow].startY - y) + abs(_placementRows[botRow].startX + _placementRows[botRow].siteWidth*botleftCol - x);
        if (minDistance == -1 || distance < minDistance)
        {
            minDistance = distance;
            nearestSite = _sites[botRow][botleftCol];
        }
    }
    return nearestSite;
}

void SiteMap::place(Cell* cell)
{
    const int cellWidth = cell->getWidth();
    const int cellHeight = cell->getHeight();
    const int cellX = cell->getX();
    const int cellY = cell->getY();
    const int cellRightX = cellX + cellWidth;
    const int cellUpY = cellY + cellHeight;
    std::vector<Site*> sites = getSitesOfCell(cellX, cellY, cellRightX, cellUpY);
        
    for (Site* site : sites)
    {
        site->place(cell);
    }
}

void SiteMap::removeCell(Cell* cell)
{
    const int cellWidth = cell->getWidth();
    const int cellHeight = cell->getHeight();
    const int cellX = cell->getX();
    const int cellY = cell->getY();
    const int cellRightX = cellX + cellWidth;
    const int cellUpY = cellY + cellHeight;
    std::vector<Site*> sites = getSitesOfCell(cellX, cellY, cellRightX, cellUpY);
    for (Site* site : sites)
    {
        site->removeCell(cell);
    }
    return;
}

/*
Check the point is on the left down corner of a site or not
*/
bool SiteMap::onSite(int x, int y)
{   
    if (x < DIE_LOW_LEFT_X || y < DIE_LOW_LEFT_Y || x > DIE_UP_RIGHT_X || y > DIE_UP_RIGHT_Y)
    {
        return false;
    }
    if (_y2row.find(y) == _y2row.end())
    {
        return false;
    }
    int siteRow = _y2row[y];
    if (_x2col[siteRow].find(x) == _x2col[siteRow].end())
    {
        if (_hasMultiPlaceRow)
        {
            siteRow++;
            while (siteRow < int(_placementRows.size()) && _placementRows[siteRow].startY == y)
            {
                if (_x2col[siteRow].find(x) != _x2col[siteRow].end())
                {
                    return true;
                }
                siteRow++;
            }
        }
        return false;
    }
    return true;
}