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
        FF(int x, int y, std::string inst_name, LibCell* lib_cell, std::vector<std::pair<Pin*, Pin*>> dqpairs, std::vector<Pin*> clks);
        ~FF();

        inline int getBit() const { return _lib_cell->bit; }
        inline double getQDelay() const { return _lib_cell->qDelay; }
        inline double getPower() const { return _lib_cell->power; }
        inline double getCostPA() const { return _lib_cell->costPA; }
        inline Pin* getClkPin() const { return _clkPin; }
        inline int getClkDomain() const { return clkDomain; }
        std::vector<std::pair<Pin*, Pin*>> getDQpairs();
        double getTotalNegativeSlack();
        int getNSPinCount();

        void setClkDomain(int clkDomain);
    private:
        int clkDomain;
};