#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include "param.h"
#include "util.h"
#include "Comb.h"
#include "FF.h"
#include "Net.h"
#include "Pin.h"
#include "Site.h"

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
        void solve();

        void display();
        
    private:
        // lib
        std::vector<Comb*> _combsLibList;
        std::vector<FF*> _ffsLibList;
        std::unordered_map<std::string, Comb*> _combsLibMap;
        std::unordered_map<std::string, FF*> _ffsLibMap;
        // I/O
        std::vector<Pin*> _inputPins;
        std::vector<Pin*> _outputPins;
        std::unordered_map<std::string, Pin*> _inputPinsMap;
        std::unordered_map<std::string, Pin*> _outputPinsMap;
        // instance
        std::vector<Comb*> _combs;
        std::vector<FF*> _ffs;
        std::vector<Net*> _nets;
        std::unordered_map<std::string, Net*> _netsMap;
        std::unordered_map<std::string, Comb*> _combsMap;
        std::unordered_map<std::string, FF*> _ffsMap;
        // placement
        std::vector<PlacementRows> _placementRows;
        std::vector<Site*> _sites;
};