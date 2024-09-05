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
        void removeCell(Cell* cell);

        inline bool isOccupied() const { return !_cells.empty(); }
        inline int getX() const { return _x; }
        inline int getY() const { return _y; }
        inline int getWidth() const { return _width; }
        inline int getHeight() const { return _height; }

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
        std::vector<Site*> getSitesOfCell(int leftDownX, int leftDownY, int rightUpX, int rightUpY);
        std::vector<Site*> getSitesInBlock(int leftDownX, int leftDownY, int rightUpX, int rightUpY);
        inline std::vector<std::vector<Site*>> getSiteRows() const { return _sites; }

        Site* getNearestSite(int x, int y);

        void place(Cell* cell);
        void removeCell(Cell* cell);

        bool onSite(int x, int y);
    private:
        bool _hasMultiPlaceRow;
        std::vector<PlacementRows> _placementRows;
        std::vector<std::vector<Site*>> _sites;
        std::unordered_map<int, int> _y2row;
        std::vector<std::unordered_map<int, int>> _x2col;

        // Helper functions
        int getFirstLargerRow(int y);
        int getFirstLargerColInRow(int row, int x);
};