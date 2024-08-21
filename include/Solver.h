#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <cmath>
#include <climits>
#include <iomanip>
#include <chrono>
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
class Legalizer;

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
        bool check();
        void dump(std::string filename) const;
        void dump(std::vector<std::string>& vecStr) const;
        void dump_best(std::string filename) const;
        void report();

        void display();
        
        // friend
        friend class Renderer;
        friend class Legalizer;
    private:
        // lib
        std::vector<LibCell*> _combsLibList;
        std::vector<LibCell*> _ffsLibList;
        std::unordered_map<std::string, LibCell*, CIHash, CIEqual> _combsLibMap;
        std::unordered_map<std::string, LibCell*, CIHash, CIEqual> _ffsLibMap;
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
        std::vector<std::vector<FF*>> _ffs_clkdomains;
        // placement
        std::vector<PlacementRows> _placementRows;
        BinMap* _binMap;
        SiteMap* _siteMap;
        int uniqueNameCounter = 0;

        // Cost Calculation

        double _initCost;
        double _currCost;
        double calDiffCost(double oldSlack, double newSlack);
        double calCostMoveD(Pin* movedDPin, int sourceX, int sourceY, int targetX, int targetY, bool update);
        double calCostMoveQ(Pin* movedQPin, int sourceX, int sourceY, int targetX, int targetY, bool update);
        double calCostChangeQDelay(Pin* changedQPin, double diffQDelay, bool update);
        double calCostMoveFF(FF* movedFF, int sourceX, int sourceY, int targetX, int targetY, bool update);
        double calCostBankFF(FF* ff1, FF* ff2, LibCell* targetFF, int targetX, int targetY, bool update);
        void resetSlack(bool check = false);
        
        // Modify Cell
        
        void placeCell(Cell* cell);
        void placeCell(Cell* cell, int x, int y);
        void removeCell(Cell* cell);
        void moveCell(Cell* cell, int x, int y);
        
        // Modify FF
        
        void addFF(FF* ff);
        void deleteFF(FF* ff);
        void bankFFs(FF* ff1, FF* ff2, LibCell* targetFF, int x, int y);
        
        // Trivial
        
        std::string makeUniqueName();
        void checkCLKDomain();
        
        // Helper
        
        bool isOverlap(Cell* cell1, Cell* cell2);
        bool isOverlap(int x1, int y1, Cell* cell1, Cell* cell2);
        bool isOverlap(int x1, int y1, int w1, int h1, Cell* cell2);
        bool placeable(Cell* cell);
        bool placeable(Cell* cell, int x, int y);
        bool placeable(LibCell* libCell, int x, int y);
        bool placeable(Cell* cell, int x, int y, int& move_distance);
        void constructFFsCLKDomain();
        std::vector<int> regionQuery(std::vector<FF*> ffs, long unsigned int idx, int radius);
        double calCost();
        // Main Algorithms
        
        // 1. Debank all FFs
        void debankAll();
        // 2. Force-directed placement
        void forceDirectedPlaceFFLock(const int ff_idx, std::vector<bool>& locked, std::vector<char>& lock_cnt, int& lock_num);
        void forceDirectedPlacement();
        void findForceDirectedPlacementBankingFFs(FF* ff1, FF* ff2, int& result_x, int& result_y);
        void iterativePlacement();
        // 3. Clustering in each clock domain
        std::vector<std::vector<FF*>> clusteringFFs(long unsigned int clkdomain_idx);
        // 4. Greedy banking
        void greedyBanking(std::vector<std::vector<FF*>> clusters);
        double cal_banking_gain(FF* ff1, FF* ff2, LibCell* targetFF, int& result_x, int& result_y);
        // 5. Legalization
        Legalizer* _legalizer;
        void iterativePlacementLegal();
        // 6. Fine-tuning
        void fineTune();

        // State saving
        std::vector<std::string> _stateNames;
        std::vector<double> _stateCosts;
        std::vector<double> _stateTimes;
        std::vector<std::vector<std::string>> _stateDumps;
        std::vector<bool> _stateLegal;
        size_t _bestStateIdx;
        double _bestCost = -1;
        void saveState(std::string stateName, bool legal = true);

        // Checker
        bool checkOverlap();
        bool checkFFInDie();
        bool checkFFOnSite();
};