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
        // placement
        std::vector<PlacementRows> _placementRows;
        std::vector<Site*> _sites;
};