#include <queue>
#include <utility>
#include "Site.h"
#include "param.h"
#include "Solver.h"
#include "Cell.h"
#include "Bin.h"

bool isPlacable(std::vector<Site*> sites)
{
    for (Site* site : sites)
    {
        if (site->isOccupied())
        {
            return false;
        }
    }
    return true;
}

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

bool Site::removeCell(Cell* cell)
{
    if (std::find(_cells.begin(), _cells.end(), cell) != _cells.end())
    {
        _cells.erase(std::remove(_cells.begin(), _cells.end(), cell), _cells.end());
        return true;
    }
    return false;
}

bool Site::isOccupied()
{
    return !_cells.empty();
}

bool Site::isOccupiedByComb()
{
    for (Cell* cell : _cells)
    {
        if (cell->getCellType() == CellType::COMB)
        {
            return true;
        }
    }
    return false;
}

bool Site::isOccupiedByCrossRowCell()
{
    for (Cell* cell : _cells)
    {
        if (cell->getY() != _y)
        {
            return true;
        }
    }
    return false;
}

bool Site::isOverLapping()
{
    return _cells.size() > 1;
}

std::vector<Cell*> Site::getCell()
{
    return this->_cells;
}

int Site::getX()
{
    return this->_x;
}

int Site::getY()
{
    return this->_y;
}

int Site::getWidth()
{
    return this->_width;
}

int Site::getHeight()
{
    return this->_height;
}

int Site::getNumCells()
{
    return this->_cells.size();
}

SiteMap::SiteMap()
{
}

SiteMap::SiteMap(std::vector<PlacementRows> placementRows)
{
    _placementRows = placementRows;
    _sites.resize(placementRows.size());
    _x2col.resize(placementRows.size());
    const int nPlacementRows = placementRows.size();
    for (int i = 0; i < nPlacementRows; i++)
    {
        const PlacementRows& row = placementRows[i];
        int siteX = row.startX;
        _y2row[row.startY] = i;
        for (int j = 0; j < row.numSites; j++)
        {
            Site* site = new Site(siteX, row.startY, row.siteWidth, row.siteHeight);
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

std::vector<Site*> SiteMap::getSites(int leftDownX, int leftDownY, int rightUpX, int rightUpY)
{
    if (leftDownX < DIE_LOW_LEFT_X || leftDownY < DIE_LOW_LEFT_Y || rightUpX > DIE_UP_RIGHT_X || rightUpY > DIE_UP_RIGHT_Y)
    {
        // std::cerr << "Error: SiteMap::getSites() - out of die boundary" << std::endl;
        return std::vector<Site*>();    
    }
    if (_y2row.find(leftDownY) == _y2row.end())
    {
        // std::cerr << "Error: SiteMap::getSites() - not on site" << std::endl;
        return std::vector<Site*>();    
    }
    if (_x2col[_y2row[leftDownY]].find(leftDownX) == _x2col[_y2row[leftDownY]].end())
    {
        // std::cerr << "Error: SiteMap::getSites() - not on site" << std::endl;
        return std::vector<Site*>();
    }
    std::vector<Site*> sites;
    const int startRow = _y2row[leftDownY];
    const int endRow = getFirstLargerRow(rightUpY);
    for (int row = startRow; row < endRow; row++)
    {
        const int startCol = getFirstLargerColInRow(row, leftDownX);
        const int endCol = getFirstLargerColInRow(row, rightUpX);
        if(startCol == endCol)
        {
            sites.push_back(_sites[row][startCol]);
            continue;
        }
        for (int col = startCol; col < endCol; col++)
        {
            sites.push_back(_sites[row][col]);
        }
    }
    return sites;
}

std::vector<std::vector<Site*>> SiteMap::getSiteRows()
{
    return _sites;
}

int SiteMap::getFirstLargerRow(int y)
{
    if (y < DIE_LOW_LEFT_Y || y > DIE_UP_RIGHT_Y)
    {
        std::cerr << "Error: SiteMap::getFirstLargerRow() - out of die boundary" << std::endl;
        exit(1);
    }
    int row = _placementRows.size();
    // binary search
    int left = 0;
    int right = row-1;
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
        exit(1);
    }
    int col = (x - _placementRows[row].startX) / _placementRows[row].siteWidth;
    col = std::max(0, col);
    col = std::min(col, _placementRows[row].numSites-1);
    return col;
}

Site* SiteMap::getNearestSite(int x, int y)
{
    if (x < DIE_LOW_LEFT_X || y < DIE_LOW_LEFT_Y || x > DIE_UP_RIGHT_X || y > DIE_UP_RIGHT_Y)
    {
        std::cerr << "Error: SiteMap::getNearestSite() - out of die boundary" << std::endl;
        exit(1);
    }
    int distance;
    int minDistance = -1;
    Site* nearestSite = nullptr;
    int topRow = getFirstLargerRow(y);
    int botRow = topRow - 1;
    int leftCol = getFirstLargerColInRow(topRow, x);
    int rightCol = leftCol - 1;
    if (topRow < int(_placementRows.size()) && leftCol < _placementRows[topRow].numSites)
    {
        distance = abs(_placementRows[topRow].startY - y);
        if (minDistance == -1 || distance < minDistance)
        {
            minDistance = distance;
            nearestSite = _sites[topRow][leftCol];
        }
    }
    if (botRow >= 0 && rightCol >= 0)
    {
        distance = abs(_placementRows[botRow].startY + _placementRows[botRow].siteHeight - y);
        if (minDistance == -1 || distance < minDistance)
        {
            minDistance = distance;
            nearestSite = _sites[botRow][rightCol];
        }
    }
    if (topRow < int(_placementRows.size()) && rightCol >= 0)
    {
        distance = abs(_placementRows[topRow].startX + _placementRows[topRow].siteWidth - x);
        if (minDistance == -1 || distance < minDistance)
        {
            minDistance = distance;
            nearestSite = _sites[topRow][rightCol];
        }
    }
    if (botRow >= 0 && leftCol < _placementRows[botRow].numSites)
    {
        distance = abs(_placementRows[botRow].startX - x);
        if (minDistance == -1 || distance < minDistance)
        {
            minDistance = distance;
            nearestSite = _sites[botRow][leftCol];
        }
    }
    return nearestSite;
}

Site* SiteMap::getNearestAvailableSite(int x, int y, Cell* cell)
{
    // BFS from the start site
    std::queue<std::pair<int,int>> q;
    std::vector<std::vector<bool>> visited(_placementRows.size(), std::vector<bool>(_placementRows[0].numSites, false));
    int cellWidth = cell->getWidth();
    int cellHeight = cell->getHeight();
    Site* startSite = getNearestSite(x, y);
    int siteX_idx = _x2col[_y2row[startSite->getY()]][startSite->getX()];
    int siteY_idx = _y2row[startSite->getY()];
    q.push(std::make_pair(siteX_idx, siteY_idx));

    while (!q.empty())
    {
        std::pair<int,int> cur = q.front();
        q.pop();
        if(visited[cur.second][cur.first])
            continue;
        visited[cur.second][cur.first] = true;
        int curX_idx = cur.first;
        int curY_idx = cur.second;
        std::vector<Site*> sites = getSites(_sites[curY_idx][curX_idx]->getX(), _sites[curY_idx][curX_idx]->getY(), _sites[curY_idx][curX_idx]->getX() + cellWidth, _sites[curY_idx][curX_idx]->getY() + cellHeight);
        if (isPlacable(sites))
        {
            return _sites[curY_idx][curX_idx];
        }
        if (curX_idx + 1 < _placementRows[curY_idx].numSites)
        {
            q.push(std::make_pair(curX_idx + 1, curY_idx));
        }
        if (curY_idx + 1 < int(_placementRows.size()))
        {
            q.push(std::make_pair(curX_idx, curY_idx + 1));
        }
        if (curX_idx > 0)
        {
            q.push(std::make_pair(curX_idx - 1, curY_idx));
        }
        if (curY_idx > 0)
        {
            q.push(std::make_pair(curX_idx, curY_idx - 1));
        }
    }
    return nullptr;
}

bool SiteMap::place(Cell* cell, bool allowOverlap)
{
    const int cellWidth = cell->getWidth();
    const int cellHeight = cell->getHeight();
    const int cellX = cell->getX();
    const int cellY = cell->getY();
    const int cellRightX = cellX + cellWidth;
    const int cellUpY = cellY + cellHeight;
    std::vector<Site*> sites = getSites(cellX, cellY, cellRightX, cellUpY);
    // if(sites.empty())
    // {
    //     // cout cell info
    //     std::cout << "Cellx: " << cellX << " Celly: " << cellY << " CellWidth: " << cellWidth << " CellHeight: " << cellHeight << std::endl;
    // }
    if(!allowOverlap)
    {
        for (Site* site : sites)
        {
            if (site->isOccupied())
            {
                return true;
            }
        }
    }
        
    for (Site* site : sites)
    {
        site->place(cell);
        cell->addSite(site);
    }
    return false;
}

void SiteMap::removeCell(Cell* cell)
{
    const int cellWidth = cell->getWidth();
    const int cellHeight = cell->getHeight();
    const int cellX = cell->getX();
    const int cellY = cell->getY();
    const int cellRightX = cellX + cellWidth;
    const int cellUpY = cellY + cellHeight;
    std::vector<Site*> sites = getSites(cellX, cellY, cellRightX, cellUpY);
    for (Site* site : sites)
    {
        if(site->removeCell(cell))
            cell->removeSite(site);
    }
    return;
}

bool SiteMap::moveCell(Cell* cell, int x, int y, bool allowOverlap)
{
    removeCell(cell);
    cell->setX(x);
    cell->setY(y);
    return place(cell, allowOverlap);
}

bool SiteMap::moveCell(Cell* cell, bool allowOverlap)
{
    const int x = cell->getX();
    const int y = cell->getY();
    return moveCell(cell, x, y, allowOverlap);
}

bool SiteMap::checkOverlap()
{
    bool isOverlapping = false;
    for (const auto& row : _sites)
    {
        for (Site* site : row)
        {
            if (site->isOverLapping())
            {
                isOverlapping = true;
                // output overlapping sites
                std::cerr << "Overlapping site at (" << site->getX() << ", " << site->getY() << ")" << std::endl;
            }
        }
    }
    return isOverlapping;
}