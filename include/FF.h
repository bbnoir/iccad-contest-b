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

        int getBit() const { return _lib_cell->bit; }
        int getOccupiedBit() const { return _outputPins.size(); }
        double getQDelay() const { return _lib_cell->qDelay; }
        double getPower() const { return _lib_cell->power; }
        double getTotalNegativeSlack();
        double getTotalSlack();
        double getCostPA() const { return _lib_cell->costPA; }
        int getNSPinCount();
        Pin* getClkPin() const { return _clkPin; }
        std::vector<std::pair<Pin*, Pin*>> getDQpairs();
        int getClkDomain() const { return clkDomain; }

        void setClkDomain(int clkDomain);
    private:
        int clkDomain;
};