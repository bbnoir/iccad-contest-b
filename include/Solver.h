#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <climits>
#include "param.h"
#include "util.h"

class LibCell;
class Pin;
class Cell;
class Comb;
class FF;
class Net;
class Site;
class BinMap;
class SiteMap;

struct PlacementRows
{
    int startX;
    int startY;
    int siteWidth;
    int siteHeight;
    int numSites;
};

class Solver
{
    public:
        Solver();
        ~Solver();

        void parse_input(std::string filename);
        void init_placement();
        void solve();

        void display();
        
        // friend
        friend class Renderer;
    private:
        // lib
        std::vector<LibCell*> _combsLibList;
        std::vector<LibCell*> _ffsLibList;
        std::unordered_map<std::string, LibCell*, CIHash, CIEqual> _combsLibMap;
        std::unordered_map<std::string, LibCell*, CIHash, CIEqual> _ffsLibMap;
        LibCell* _baseFF;
        // I/O
        std::vector<Pin*> _inputPins;
        std::vector<Pin*> _outputPins;
        std::unordered_map<std::string, Pin*, CIHash, CIEqual> _inputPinsMap;
        std::unordered_map<std::string, Pin*, CIHash, CIEqual> _outputPinsMap;
        // instance
        std::vector<Comb*> _combs;
        std::vector<FF*> _ffs;
        std::vector<Net*> _nets;
        std::unordered_map<std::string, Net*, CIHash, CIEqual> _netsMap;
        std::unordered_map<std::string, Comb*, CIHash, CIEqual> _combsMap;
        std::unordered_map<std::string, FF*, CIHash, CIEqual> _ffsMap;
        // placement
        std::vector<PlacementRows> _placementRows;
        BinMap* _binMap;
        SiteMap* _siteMap;
        int uniqueNameCounter = 0;
        // functions
        bool placeCell(Cell* cell, bool allowOverlap = false);
        void removeCell(Cell* cell);
        bool moveCell(Cell* cell, int x, int y, bool allowOverlap = false);
        void addFF(FF* ff); // add FF to _ffs and _ffsMap
        void deleteFF(FF* ff); // delete FF from _ffs and _ffsMap
        void bankFFs(FF* ff1, FF* ff2, LibCell* targetFF);
        std::string makeUniqueName();
        double cal_total_hpwl();
        // algorithms
        // 1. Debank all FFs
        void chooseBaseFF();
        void debankAll();
        // 2. Force-directed placement
        void forceDirectedPlaceFF(FF* ff);
        void forceDirectedPlaceFFLock(const int ff_idx, std::vector<bool>& locked, std::vector<char>& lock_cnt, int& lock_num);
        void forceDirectedPlacement();
        // 3. Legalization
        void legalize();

};