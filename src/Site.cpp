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
    this->_cell = cell;
}

void Site::removeCell()
{
    this->_cell = nullptr;
}

Cell* Site::getCell()
{
    return this->_cell;
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

std::vector<Site*> SiteMap::getSites(int leftDownX, int leftDownY, int rightUpX, int rightUpY)
{
    if (leftDownX < DIE_LOW_LEFT_X || leftDownY < DIE_LOW_LEFT_Y || rightUpX > DIE_UP_RIGHT_X || rightUpY > DIE_UP_RIGHT_Y)
    {
        std::cerr << "Error: SiteMap::getSites() - out of die boundary" << std::endl;
        exit(1);
    }
    if (_y2row.find(leftDownY) == _y2row.end())
    {
        std::cerr << "Error: SiteMap::getSites() - invalid leftDownY" << std::endl;
        exit(1);
    }
    if (_x2col[_y2row[leftDownY]].find(leftDownX) == _x2col[_y2row[leftDownY]].end())
    {
        std::cerr << "Error: SiteMap::getSites() - invalid leftDownX" << std::endl;
        exit(1);
    }
    std::vector<Site*> sites;
    int startRow = _y2row[leftDownY];
    int startCol = _x2col[startRow][leftDownX];
    const int nPlacementRows = _placementRows.size();
    for (; startRow < nPlacementRows && _placementRows[startRow].startY < rightUpY; startRow++)
    {
        const int nSites = _sites[startRow].size();
        for (int col = startCol; col < nSites; col++)
        {
            Site* site = _sites[startRow][col];
            if (site->getX() >= rightUpX)
            {
                break;
            }
            sites.push_back(site);
        }
        startCol = 0;
    }

    return sites;
}