#pragma once
#include <string>
#include "Cell.h"
#include "Pin.h"

class FF : public Cell
{
    public:
        using Cell::Cell;
        FF(int x, int y, std::string inst_name, LibCell* lib_cell, std::pair<Pin*, Pin*> dqpair, Pin* clk);
        FF(int x, int y, std::string inst_name, LibCell* lib_cell, std::vector<std::pair<Pin*, Pin*>> dqpairs, Pin* clk);
        FF(int x, int y, std::string inst_name, LibCell* lib_cell, std::vector<std::pair<Pin*, Pin*>> dqpairs, std::vector<Pin*> clks, bool once_sa = false);
        ~FF();

        int getBit();
        int getOccupiedBit();
        double getQDelay();
        double getPower();
        double getTotalNegativeSlack();
        double getTotalSlack();
        double getCostPA();
        int getNSPinCount();
        Pin* getClkPin();
        std::vector<std::pair<Pin*, Pin*>> getDQpairs();
        int getClkDomain();
        inline bool isOnceSA() { return _onceSA; }

        void setClkDomain(int clkDomain);
    private:
        int clkDomain;
        bool _onceSA = false;
};