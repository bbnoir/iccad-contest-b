#pragma once
#include <string>
#include "Cell.h"
#include "Pin.h"

class FF : public Cell
{
    public:
        using Cell::Cell;
        FF(int x, int y, std::string inst_name, LibCell* lib_cell, std::pair<Pin*, Pin*> dqpair, Pin* clk);
        FF(int x, int y, std::string inst_name, LibCell* lib_cell, std::vector<std::pair<Pin*, Pin*>> dqpairs, std::vector<Pin*> clks);
        ~FF();

        int getBit();
        int getOccupiedBit();
        double getQDelay();
        Pin* getClkPin();
        std::vector<std::pair<Pin*, Pin*>> getDQpairs();
        int getClkDomain();

        void setClkDomain(int clkDomain);
    private:
        int clkDomain;
};