#include "LegalPlacer.h"
#include "Solver.h"
#include "FF.h"
#include "Site.h"

SubRow::SubRow(std::vector<Site*> sites){
    _sites = sites;
    centerX = sites.at(0)->getX() + (sites.at(sites.size()-1)->getX() - sites.at(0)->getX())/2;
    rowY = sites.at(0)->getY();
}

SubRow::~SubRow(){
}

LegalPlacer::LegalPlacer(Solver* solver){
    _solver = solver;
}

LegalPlacer::~LegalPlacer(){
}

void LegalPlacer::removeAllFFs(){
    for(size_t i = 0;i < _ffs.size();i++){
        _solver->removeCell(_ffs.at(i));
    }
}

void LegalPlacer::generateSubRows(){
    // Row are separated by Combs
    _subRows.clear();
    std::vector<std::vector<Site*>> siteRow = _solver->_siteMap->getSiteRows();
    std::vector<Site*> subRow;
    for(size_t i = 0;i < siteRow.size();i++){
        for(size_t j = 0;j < siteRow.at(i).size();j++){
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

std::vector<int> LegalPlacer::getNearSubRows(FF* ff, int min_distance, int max_distance)
{
    std::vector<int> nearSubRows;
    const int centerX = ff->getX() + ff->getWidth()/2;
    const int centerY = ff->getY() + ff->getHeight()/2;
    for(long unsigned int i = 0;i < _subRows.size();i++){
        SubRow subRow = _subRows.at(i);
        // calculate Manhattan distance
        int manhattanDistance = abs(centerX - subRow.centerX) + abs(centerY - subRow.rowY);
        if(manhattanDistance <= max_distance && manhattanDistance >= min_distance){
            nearSubRows.push_back(i);
        }
    }
    return nearSubRows;
}


double LegalPlacer::placeRow(FF* ff, int subRowIndex, bool trial){
    SubRow subRow = _subRows[subRowIndex];
    const int startX = subRow._sites.at(0)->getX();
    const int endX = subRow._sites.at(subRow._sites.size() - 1)->getX();
    const int rowY = subRow._sites.at(0)->getY();
    const int siteWidth = subRow._sites.at(0)->getWidth();    

    // Find the best position
    int bestX = -1;
    double bestCost = INFINITY;
    for(int i = startX;i <= endX - ff->getWidth();i+=siteWidth){
        int move = 0;
        if(!_solver->placeable(ff, i, rowY, move)){
            int delta = (ceil(1.*move/siteWidth)- 1)*siteWidth;
            i += std::max(0, delta);
            continue;
        }
        // Calculate cost(displacement in Manhattan distance)
        double cost = (abs(ff->getX() - i) + abs(ff->getY() - rowY));
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

void LegalPlacer::legalize(){
    _ffs = _solver->_ffs;
    double totalMove = 0;
    removeAllFFs();
    // Sort FFs by x
    std::sort(_ffs.begin(), _ffs.end(), [](FF* a, FF* b){
        if(a->getX() == b->getX())
            return a->getBit() > b->getBit();
        else
            return a->getX() < b->getX();
    });

    std::vector<int> orphans;
    // HYPER
    int searchDistance = (DIE_UP_RIGHT_Y-DIE_LOW_LEFT_Y)/50;
    if (searchDistance < 1)
    {
        searchDistance = DIE_UP_RIGHT_Y-DIE_LOW_LEFT_Y;
    }

    for(size_t i = 0;i < _ffs.size();i++){
        double cost_min = INFINITY;
        int best_subrow = -1;
        std::vector<int> nearSubRows = getNearSubRows(_ffs[i], -1, searchDistance);

        for(size_t j = 0;j < nearSubRows.size();j++){
            double cost = placeRow(_ffs[i], nearSubRows[j], true);
            if(cost < cost_min){
                cost_min = cost;
                best_subrow = nearSubRows[j];
            }
        }
        if(best_subrow != -1){
            placeRow(_ffs[i], best_subrow, false);      
            totalMove += cost_min;
        }else{
            orphans.push_back(i);
        }
    }
    // Place orphans
    for(long unsigned int i = 0;i < orphans.size();i++){
        double cost_min = INFINITY;
        int best_subrow = -1;
        int min_distance = searchDistance;
        int max_distance = 3*searchDistance;
        while(best_subrow == -1 && max_distance < (DIE_UP_RIGHT_Y-DIE_LOW_LEFT_Y)){
            std::vector<int> nearSubRows = getNearSubRows(_ffs[orphans[i]], min_distance, max_distance);
            #pragma omp parallel for num_threads(4)
            for(long unsigned int j = 0;j < nearSubRows.size();j++){
                double cost = placeRow(_ffs[orphans[i]], nearSubRows[j], true);
                #pragma omp critical
                if(cost < cost_min){
                    cost_min = cost;
                    best_subrow = nearSubRows[j];
                }
            }
            // Increase search distance
            min_distance = max_distance;
            max_distance += 2*searchDistance;
        }
        totalMove += cost_min;
        if(best_subrow != -1)
            placeRow(_ffs[orphans[i]], best_subrow, false);
    }
}