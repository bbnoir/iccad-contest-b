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

std::vector<int> Legalizer::getNearSubRows(int ffIndex, int min_distance, int max_distance){
    std::vector<int> nearSubRows;
    FF* ff = _ffs.at(ffIndex);
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
        int move = 0;
        if(!_solver->placeable(ff, i, rowY, move)){
            i += (ceil(move/siteWidth)- 1)*siteWidth;
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
        std::vector<int> nearSubRows = getNearSubRows(i, -1, searchDistance);
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
        while(best_subrow == -1){
            std::vector<int> nearSubRows = getNearSubRows(orphans[i], min_distance, max_distance);
            for(long unsigned int j = 0;j < nearSubRows.size();j++){
                double cost = placeRow(orphans[i], nearSubRows[j], true);
                if(cost < cost_min){
                    cost_min = cost;
                    best_subrow = nearSubRows[j];
                }
            }
            // Increase search distance
            min_distance = max_distance;
            max_distance += searchDistance;
        }
        totalCost += cost_min;
        if(best_subrow != -1)
            placeRow(orphans[i], best_subrow, false);
        else
            std::cout<<"There is no place for orphan "<<orphans[i]<<std::endl;
    }

    std::cout << "Legalizing done." << std::endl;
    std::cout << "Total cost: " << totalCost << std::endl;
    std::cout << (orphans.size()/double(_ffs.size()))*100 << "% FFs are orphan." << std::endl;
}

void Legalizer::fineTune()
{
    std::cout << "Fine tuning..." << std::endl;

    int kernel_size = 5;
    int stride_x = 1;
    int stride_y = 1;
    int stride_x_width = stride_x * BIN_WIDTH;
    int stride_y_height = stride_y * BIN_HEIGHT;
    int num_x = (_solver->_binMap->getNumBinsX() - kernel_size) / stride_x + 1;
    int num_y = (_solver->_binMap->getNumBinsY() - kernel_size) / stride_y + 1;
    for(int y = 0; y < num_y; y+=stride_y)
    {
        for(int x = 0; x < num_x; x+=stride_x)
        {
            // continue if the bins utilization rate is less than threshold
            std::vector<Bin*> bins = _solver->_binMap->getBinsBlocks(x, y, kernel_size, kernel_size);
            bool is_over_max_util = false;
            for(long unsigned int i = 0; i < bins.size(); i++)
            {
                if(bins[i]->isOverMaxUtil())
                {
                    is_over_max_util = true;
                    break;
                }
            }
            if(!is_over_max_util)
                continue;

            std::cout << "Fine tuning block (" << x << ", " << y << ")" << std::endl;
            std::vector<FF*> ffs_local = _solver->_binMap->getFFsInBinsBlocks(x, y, kernel_size, kernel_size);
            // TODO: get sites in the block
            std::vector<Site*> sites = _solver->_siteMap->getSitesInBlock(x*stride_x_width, y*stride_y_height, (x+kernel_size)*stride_x_width, (y+kernel_size)*stride_y_height);
            std::cout << "Number of FFs in the block: " << ffs_local.size() << std::endl;
            std::cout << "Number of sites in the block: " << sites.size() << std::endl;
            // remove ffs in the block
            for(long unsigned int i = 0; i < ffs_local.size(); i++)
            {
                _solver->removeCell(ffs_local[i]);
            }
            for(long unsigned int i = 0; i < ffs_local.size(); i++)
            {
                FF* ff = ffs_local[i];
                double cost_min = INFINITY;
                int best_site = -1;
                for(long unsigned int j = 0; j < sites.size(); j++)
                {
                    if(!_solver->placeable(ff, sites[j]->getX(), sites[j]->getY()))
                        continue;
                    
                    // TODO: cost is depend on the slack and bin utilization rate
                    double cost = abs(ff->getX() - sites[j]->getX()) + abs(ff->getY() - sites[j]->getY());
                    
                    if(cost < cost_min)
                    {
                        cost_min = cost;
                        best_site = j;
                    }
                }
                if(best_site != -1)
                {
                    _solver->placeCell(ff, sites[best_site]->getX(), sites[best_site]->getY());
                    // std::cout << "FF " << ff->getInstName() << " is placed at (" << sites[best_site]->getX() << ", " << sites[best_site]->getY() << ")" << std::endl;
                }
            }
        }
    }
    // TODO: place orphans

    std::cout << "Fine tuning done." << std::endl;
}