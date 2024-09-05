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


class LegalPlacer{
    private:
        Solver* _solver;
        std::vector<FF*> _ffs;
        std::vector<SubRow> _subRows;
        
        void removeAllFFs();
        
        std::vector<int> getNearSubRows(FF* ff, int min_distance, int max_distance);
        double placeRow(FF* ff, int subRowIndex, bool trial = false);
    public:
        LegalPlacer(Solver* solver);
        ~LegalPlacer();

        void legalize();
        void generateSubRows();
};