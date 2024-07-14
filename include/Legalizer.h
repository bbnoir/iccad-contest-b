#pragma once
#include <iostream>
#include <vector>
#include <climits>
#include <algorithm>

class Solver;
class FF;
class Site;

struct SubRow{
    int centerX;
    int rowY;
    std::vector<Site*> _sites;
    SubRow(std::vector<Site*> sites);
    ~SubRow();
};


class Legalizer{
    private:
        Solver* _solver;
        std::vector<FF*> _ffs;
        std::vector<SubRow> _subRows;
        
        void removeAllFFs();
        void generateSubRows();
        std::vector<int> getNearSubRows(int ffIndex, int distance);
        double placeRow(int ffIndex, int subRowIndex, bool trial = false);
    public:
        Legalizer(Solver* solver);
        ~Legalizer();

        void legalize();

};