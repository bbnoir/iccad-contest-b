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
        void removeCell();

        bool isOccupied();
        Cell* getCell();
        int getX();
        int getY();
        int getWidth();
        int getHeight();

    private:
        int _x;
        int _y;
        int _width;
        int _height;
        Cell* _cell;
};

class SiteMap
{
    public:
        SiteMap();
        SiteMap(std::vector<PlacementRows> placementRows);

        std::vector<Site*> getSites();
        std::vector<Site*> getSites(int leftDownX, int leftDownY, int rightUpX, int rightUpY);

    private:
        std::vector<PlacementRows> _placementRows;
        std::vector<std::vector<Site*>> _sites;
        std::unordered_map<int, int> _y2row;
        std::vector<std::unordered_map<int, int>> _x2col;

        // Helper functions
        int getFirstLargerRow(int y);
        int getFirstLargerColInRow(int row, int x);
};