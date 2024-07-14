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

std::vector<int> Legalizer::getNearSubRows(int ffIndex, int distance){
    std::vector<int> nearSubRows;
    FF* ff = _ffs.at(ffIndex);
    int startY = ff->getY() - distance;
    int endY = ff->getY() + distance;
    int startX = ff->getX() - distance;
    int endX = ff->getX() + distance;
    for(long unsigned int i = 0;i < _subRows.size();i++){
        SubRow subRow = _subRows.at(i);
        if(subRow.rowY >= startY && subRow.rowY <= endY && subRow.centerX >= startX && subRow.centerX <= endX){
            nearSubRows.push_back(i);
        }
    }
    return nearSubRows;
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
                i += (ceil(ff->getWidth()/siteWidth)- 1)*siteWidth;
                break;
            }
        }
        // Calculate cost(displacement in Manhattan distance)
        if(cost != INFINITY){
            cost = (abs(ff->getX() - i) + abs(ff->getY() - rowY));
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
    std::cout << "Legalizing..." << std::endl;
    _ffs = _solver->_ffs;
    double totalCost = 0;
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
        std::vector<int> nearSubRows = getNearSubRows(i, searchDistance);
        for(long unsigned int j = 0;j < nearSubRows.size();j++){
            double cost = placeRow(i, nearSubRows[j], true);
            if(cost < cost_min){
                cost_min = cost;
                best_subrow = nearSubRows[j];
            }
        }
        if(best_subrow != -1){
            placeRow(i, best_subrow, false);         
            totalCost += cost_min;
            // std::cout<<"i: "<<i<<", cost: "<<cost_min<<", total cost: "<<totalCost<<std::endl;
        }else{
            orphans.push_back(i);
            // std::cout<<"i: "<<i<<" is orphan."<<std::endl;
        }
    }

    // Place orphans
    for(long unsigned int i = 0;i < orphans.size();i++){
        double cost_min = INFINITY;
        int best_subrow = -1;
        for(long unsigned int j = 0;j < _subRows.size();j++){
            double cost = placeRow(orphans[i], j, true);
            if(cost < cost_min){
                cost_min = cost;
                best_subrow = j;
            }
        }
        totalCost += cost_min;
        // std::cout<<"Orphan "<<orphans[i]<<", cost: "<<cost_min<<", total cost: "<<totalCost<<std::endl;
        if(best_subrow != -1)
            placeRow(orphans[i], best_subrow, false);
        else
            std::cout<<"There is no place for orphan "<<orphans[i]<<std::endl;
    }

    std::cout << "Legalizing done." << std::endl;
    // std::cout << "Total cost: " << totalCost << std::endl;
    // 2.59431e+09
    // std::cout << (orphans.size()/double(_ffs.size()))*100 << "% FFs are orphan." << std::endl;
}