#include "Legalizer.h"
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
    _subRows.clear();
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

std::vector<int> Legalizer::getNearSubRows(FF* ff, int min_distance, int max_distance)
{
    std::vector<int> nearSubRows;
    int centerX = ff->getX() + ff->getWidth()/2;
    int centerY = ff->getY() + ff->getHeight()/2;
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


double Legalizer::placeRow(FF* ff, int subRowIndex, bool trial){
    SubRow subRow = _subRows[subRowIndex];
    int startX = subRow._sites.at(0)->getX();
    int endX = subRow._sites.at(subRow._sites.size() - 1)->getX();
    int rowY = subRow._sites.at(0)->getY();
    int siteWidth = subRow._sites.at(0)->getWidth();    

    // Find the best position
    int bestX = -1;
    double bestCost = INFINITY;
    for(int i = startX;i <= endX - ff->getWidth();i+=siteWidth){
        int move = 0;
        if(!_solver->placeable(ff, i, rowY, move)){
            i += (ceil(1.*move/siteWidth)- 1)*siteWidth;
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

void Legalizer::legalize(){
    std::cout << "Legalizing..." << std::endl;
    _ffs = _solver->_ffs;
    double totalMove = 0;
    removeAllFFs();
    generateSubRows();
    // Sort FFs by x
    std::sort(_ffs.begin(), _ffs.end(), [](FF* a, FF* b){
        if(a->getX() == b->getX())
            return a->getBit() > b->getBit();
        else
            return a->getX() < b->getX();
    });

    std::vector<int> orphans;
    int searchDistance = (DIE_UP_RIGHT_Y-DIE_LOW_LEFT_Y)/10;

    for(long unsigned int i = 0;i < _ffs.size();i++){
        double cost_min = INFINITY;
        int best_subrow = -1;
        std::vector<int> nearSubRows = getNearSubRows(_ffs[i], -1, searchDistance);
        for(long unsigned int j = 0;j < nearSubRows.size();j++){
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
        int max_distance = 2*searchDistance;
        while(best_subrow == -1 && max_distance < (DIE_UP_RIGHT_Y-DIE_LOW_LEFT_Y)){
            std::vector<int> nearSubRows = getNearSubRows(_ffs[orphans[i]], min_distance, max_distance);
            for(long unsigned int j = 0;j < nearSubRows.size();j++){
                double cost = placeRow(_ffs[orphans[i]], nearSubRows[j], true);
                if(cost < cost_min){
                    cost_min = cost;
                    best_subrow = nearSubRows[j];
                }
            }
            // Increase search distance
            min_distance = max_distance;
            max_distance += searchDistance;
        }
        totalMove += cost_min;
        if(best_subrow != -1)
            placeRow(_ffs[orphans[i]], best_subrow, false);
        else
            std::cout<<"There is no place for orphan "<<orphans[i]<<std::endl;
    }

    std::cout << "Legalizing done." << std::endl;
    std::cout << "Total movement: " << totalMove << std::endl;
    // std::cout << (orphans.size()/double(_ffs.size()))*100 << "% FFs are orphan." << std::endl;
}

void Legalizer::placeOrphan(FF* ff)
{
    double cost_min = INFINITY;
    int best_subrow = -1;
    int searchDistance = (DIE_UP_RIGHT_Y-DIE_LOW_LEFT_Y)/10;
    int min_distance = -1;
    int max_distance = searchDistance;
    while(best_subrow == -1){
        std::vector<int> nearSubRows = getNearSubRows(ff, min_distance, max_distance);
        for(long unsigned int j = 0;j < nearSubRows.size();j++){
            double cost = placeRow(ff, nearSubRows[j], true);
            if(cost < cost_min){
                cost_min = cost;
                best_subrow = nearSubRows[j];
            }
        }
        // Increase search distance
        min_distance = max_distance;
        max_distance += searchDistance;
    }
    if(best_subrow != -1)
        placeRow(ff, best_subrow, false);
    else
        std::cout<<"There is no place for orphan"<<std::endl;
}