#pragma once
#include <vector>
#include <unordered_map>

class Cell;
class Bin;
struct PlacementRows;

class Site
{
    public:
        Site();
        Site(int x, int y, int width, int height);
        ~Site();

        void place(Cell* cell);
        bool removeCell(Cell* cell);

        bool isOccupied();
        bool isOverLapping();
        std::vector<Cell*> getCell();
        int getNumCells();
        int getX();
        int getY();
        int getWidth();
        int getHeight();

    private:
        int _x;
        int _y;
        int _width;
        int _height;
        std::vector<Cell*> _cells;
};

class SiteMap
{
    public:
        SiteMap();
        SiteMap(std::vector<PlacementRows> placementRows);

        std::vector<Site*> getSites();
        std::vector<Site*> getSites(int leftDownX, int leftDownY, int rightUpX, int rightUpY);

        Site* getNearestSite(int x, int y);

        bool place(Cell* cell, bool allowOverlap = false);
        void removeCell(Cell* cell);
        // move cell to (x, y)
        bool moveCell(Cell* cell, int x, int y, bool allowOverlap = false);
        // move cell to cell's position
        bool moveCell(Cell* cell, bool allowOverlap = false);

    private:
        std::vector<PlacementRows> _placementRows;
        std::vector<std::vector<Site*>> _sites;
        std::unordered_map<int, int> _y2row;
        std::vector<std::unordered_map<int, int>> _x2col;

        // Helper functions
        int getFirstLargerRow(int y);
        int getFirstLargerColInRow(int row, int x);
};