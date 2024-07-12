#include "Legalizer.h"
#include "Solver.h"
#include "FF.h"


SubRow::SubRow(std::vector<Site*> sites){
    _sites = sites;
}

SubRow::~SubRow(){
}

Legalizer::Legalizer(Solver* solver){
    _solver = solver;
}

Legalizer::~Legalizer(){
}

void Legalizer::removeAllFFs(){
    for(long unsigned int i = 0;i < _ffs.size();i++){
        _solver->removeCell(_ffs.at(i));
    }
}

void Legalizer::generateSubRows(){
    // Row are separated by Combs
    std::vector<std::vector<Site*>> siteRow = _solver->_siteMap->getSiteRows();
    std::vector<Site*> subRow;
    for(long unsigned int i = 0;i < siteRow.size();i++){
        for(long unsigned int j = 0;j < siteRow.at(i).size();j++){
            Site* site = siteRow.at(i).at(j);
            if(site->isOccupied()){
                if(subRow.size() > 0){
                    _subRows.push_back(SubRow(subRow));
                    subRow.clear();
                }
            }else{
                subRow.push_back(site);
            }
        }
        if(subRow.size() > 0){
            _subRows.push_back(SubRow(subRow));
            subRow.clear();
        }
    }
}

double Legalizer::placeRow(int ffIndex, int subRowIndex, bool trial){
    FF* ff = _ffs.at(ffIndex);
    SubRow subRow = _subRows[subRowIndex];
    int startX = subRow._sites.at(0)->getX();
    int endX = subRow._sites.at(subRow._sites.size() - 1)->getX();
    int rowY = subRow._sites.at(0)->getY();
    int siteWidth = subRow._sites.at(0)->getWidth();

    // Find the best position
    int bestX = -1;
    double bestCost = INFINITY;
    for(int i = startX;i <= endX - ff->getWidth();i+=siteWidth){
        double cost = 0;
        std::vector<Site*> candidateSites = _solver->_siteMap->getSites(i, rowY, i+ff->getWidth(), rowY+ff->getHeight());
        for(long unsigned int j = 0;j < candidateSites.size();j++){
            Site* site = candidateSites.at(j);
            if(site->isOccupied()){
                cost = INFINITY;
                break;
            }
        }
        // Calculate cost(displacement in Manhattan distance)
        if(cost != INFINITY){
            cost = abs(ff->getX() - i) + abs(ff->getY() - rowY);
        }
        if(cost < bestCost){
            bestCost = cost;
            bestX = i;
        }
    }
    if(trial){
        return bestCost;
    }else{
        // Place FF
        if(bestX != -1){
            _solver->placeCell(ff, bestX, rowY);
        }
        return bestCost;
    }
}

void Legalizer::legalize(){
    _ffs = _solver->_ffs;
    std::cout << "Legalizing..." << std::endl;
    unsigned long long totalCost = 0;
    removeAllFFs();
    generateSubRows();
    // Sort FFs by x
    std::sort(_ffs.begin(), _ffs.end(), [](FF* a, FF* b){
        return a->getX() < b->getX();
    });
    for(long unsigned int i = 0;i < _ffs.size();i++){
        std::cout<<"i: "<<i<<std::endl;
        double cost_min = INFINITY;
        int best_subrow = -1;
        for(long unsigned int j = 0;j < _subRows.size();j++){
            double cost = placeRow(i, j, true);
            totalCost += cost;
            if(cost < cost_min){
                cost_min = cost;
                best_subrow = j;
            }
        }
        if(best_subrow != -1)
            placeRow(i, best_subrow, false);
        else
            std::cout << "Cannot place FF " << _ffs.at(i)->getInstName() << std::endl;
    }
    std::cout << "Legalizing done." << std::endl;
    std::cout << "Total cost: " << totalCost << std::endl;
}