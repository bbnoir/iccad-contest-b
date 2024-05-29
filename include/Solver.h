#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "param.h"
#include "Comb.h"
#include "FF.h"
#include "Net.h"
#include "Pin.h"
#include "Site.h"

// cost metrics
extern double ALPHA;
extern double BETA;
extern double GAMMA;
extern double LAMBDA;
// Die info
extern int DIE_LOW_LEFT_X;
extern int DIE_LOW_LEFT_Y;
extern int DIE_UP_RIGHT_X;
extern int DIE_UP_RIGHT_Y;
// Bin info
extern int BIN_WIDTH;
extern int BIN_HEIGHT;
extern int BIN_MAX_UTIL;
// Delay info
extern double DISP_DELAY;

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
        
    private:
        std::vector<Comb*> _combs;
        std::vector<FF*> _ffs;
        std::vector<Net*> _nets;
        std::vector<Pin*> _pins;

        std::vector<PlacementRows> _placementRows;
        std::vector<Site*> _sites;
};