#include "Solver.h"
#include "Cell.h"
#include "Comb.h"
#include "FF.h"
#include "Pin.h"
#include "Site.h"
#include "Bin.h"
#include "Legalizer.h"
#ifdef _OPENMP
#include <omp.h>
const int NUM_THREADS = 4;
#endif

// cost metrics
double ALPHA;
double BETA;
double GAMMA;
double LAMBDA;
// Die info
int DIE_LOW_LEFT_X;
int DIE_LOW_LEFT_Y;
int DIE_UP_RIGHT_X;
int DIE_UP_RIGHT_Y;
// Bin info
int BIN_WIDTH;
int BIN_HEIGHT;
double BIN_MAX_UTIL;
// Delay info
double DISP_DELAY;

const std::string DUMB_CELL_NAME = "du_mb";

std::string toLower(std::string str)
{
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

Solver::Solver()
{
    _legalizer = new Legalizer(this);
}

Solver::~Solver()
{
    delete _legalizer;
    for(auto ff : _ffs)
    {
        ff->deletePins();
        delete ff;
    }
    for(auto comb : _combs)
    {
        comb->deletePins();
        delete comb;
    }
    for(auto ff : _ffsLibList)
    {
        delete ff;
    }
    for(auto comb : _combsLibList)
    {
        delete comb;
    }
    for(auto pin : _inputPins)
    {
        delete pin;
    }
    for(auto pin : _outputPins)
    {
        delete pin;
    }
    delete _binMap;
    delete _siteMap;
}

void Solver::parse_input(std::string filename)
{
    using namespace std;
    ifstream in(filename);
    if(!in.good())
    {
        cerr << "Error opening file: " << filename << endl;
        return;
    }
    string token;
    // Read the cost metrics
    in >> token >> ALPHA;
    in >> token >> BETA;
    in >> token >> GAMMA;
    in >> token >> LAMBDA;
    // Read the die info
    in >> token >> DIE_LOW_LEFT_X >> DIE_LOW_LEFT_Y >> DIE_UP_RIGHT_X >> DIE_UP_RIGHT_Y;
    // Read I/O info
    int numInputs, numOutputs;
    in >> token >> numInputs;
    for(int i = 0; i < numInputs; i++)
    {
        string name;
        int x, y;
        in >> token >> name >> x >> y;
        _inputPins.push_back(new Pin(PinType::INPUT, x, y, name, nullptr));
        _inputPinsMap[name] = _inputPins.back();
    }
    in >> token >> numOutputs;
    for(int i = 0; i < numOutputs; i++)
    {
        string name;
        int x, y;
        in >> token >> name >> x >> y;
        _outputPins.push_back(new Pin(PinType::OUTPUT, x, y, name, nullptr));
        _outputPinsMap[name] = _outputPins.back();
    }
    // Read cell library
    while(!in.eof())
    {
        in >> token;
        if(token == "FlipFlop"){
            int bits, width, height, pinCount;
            string name;
            in >> bits >> name >> width >> height >> pinCount;
            LibCell* ff = new LibCell(CellType::FF, width, height, 0.0, 0.0, bits, name);
            for(int i = 0; i < pinCount; i++)
            {
                string pinName;
                int x, y;
                in >> token >> pinName >> x >> y;
                if (tolower(pinName[0]) == 'd') {
                    ff->inputPins.push_back(new Pin(PinType::FF_D, x, y, pinName, nullptr));
                } else if (tolower(pinName[0]) == 'q') {
                    ff->outputPins.push_back(new Pin(PinType::FF_Q, x, y, pinName, nullptr));
                } else if (tolower(pinName[0]) == 'c') {
                    ff->clkPin = new Pin(PinType::FF_CLK, x, y, pinName, nullptr);
                }
            }
            // sort the pins by name
            sort(ff->inputPins.begin(), ff->inputPins.end(), [](Pin* a, Pin* b) -> bool { return a->getName() < b->getName(); });
            sort(ff->outputPins.begin(), ff->outputPins.end(), [](Pin* a, Pin* b) -> bool { return a->getName() < b->getName(); });
            // set fanout of input pins to output pins
            _ffsLibList.push_back(ff);
            _ffsLibMap[name] = ff;
        }else if(token == "Gate"){
            int width, height, pinCount;
            string name;
            in >> name >> width >> height >> pinCount;
            LibCell* comb = new LibCell(CellType::COMB, width, height, 0.0, 0.0, 0, name);
            comb->clkPin = nullptr;
            for(int i = 0; i < pinCount; i++)
            {
                string pinName;
                int x, y;
                in >> token >> pinName >> x >> y;
                if (tolower(pinName[0]) == 'i') {
                    comb->inputPins.push_back(new Pin(PinType::GATE_IN, x, y, pinName, nullptr));
                } else if (tolower(pinName[0]) == 'o') {
                    comb->outputPins.push_back(new Pin(PinType::GATE_OUT, x, y, pinName, nullptr));
                }
            }
            _combsLibList.push_back(comb);
            _combsLibMap[name] = comb;
        }else{
            break;
        }
    }
    // Read instance info
    // Inst <instName> <libCellName> <x-coordinate> <y-coordinate>
    int instCount;
    in >> instCount;
    for(int i = 0; i < instCount; i++)
    {
        string instName, libCellName;
        int x, y;
        in >> token >> instName >> libCellName >> x >> y;
        if(_ffsLibMap.find(libCellName) != _ffsLibMap.end())
        {
            FF* ff = new FF(x, y,instName, _ffsLibMap[libCellName]);
            _ffs.push_back(ff);
            _ffsMap[instName] = ff;
        }else if(_combsLibMap.find(libCellName) != _combsLibMap.end())
        {
            Comb* comb = new Comb(x, y, instName, _combsLibMap[libCellName]);
            _combs.push_back(comb);
            _combsMap[instName] = comb;
        }
    }
    // Read net info
    int netCount;
    in >> token >> netCount;
    for(int i = 0; i < netCount; i++)
    {
        bool clknet = false;
        string netName;
        int pinCount;
        in >> token >> netName >> pinCount;
        vector<Pin*> pins;
        for(int j = 0; j < pinCount; j++)
        {
            string pinName;
            in >> token >> pinName;

            string instName = pinName.substr(0, pinName.find('/'));
            string pin;
            if(pinName.find('/') != string::npos)
                pin = pinName.substr(pinName.find('/') + 1);
            if(!clknet && (toLower(pin) == "clk" || toLower(pinName) == "clk")){
                clknet = true;
            }
            Pin* p = nullptr;
            if(_ffsMap.find(instName) != _ffsMap.end()){
                p = _ffsMap[instName]->getPin(pin);
            }else if(_combsMap.find(instName) != _combsMap.end()){
                p = _combsMap[instName]->getPin(pin);
            }else if(_inputPinsMap.find(pinName) != _inputPinsMap.end()){
                p = _inputPinsMap[pinName];
            }else if(_outputPinsMap.find(pinName) != _outputPinsMap.end()){
                p = _outputPinsMap[pinName];
            }else{
                cerr << "Error: Pin not found: " << pinName << endl;
                continue;
            }
            pins.push_back(p);
        }
        if (clknet == true)
        {
            _ffs_clkdomains.push_back(vector<FF*>());
            for (auto p : pins)
            {
                if (p->getType() == PinType::FF_CLK)
                {
                    FF* curFF = static_cast<FF*>(p->getCell());
                    _ffs_clkdomains.back().push_back(curFF);
                    curFF->setClkDomain(_ffs_clkdomains.size() - 1);
                }
            }
        }
        for(size_t i = 0; i < pins.size(); i++)
        {
            if(pins[i]->getType() == PinType::FF_Q || pins[i]->getType() == PinType::GATE_OUT || pins[i]->getType() == PinType::INPUT)
            {
                for(size_t j = 0; j < pins.size(); j++)
                {
                    if(i == j)
                        continue;
                    pins[j]->setFaninPin(pins[i]);
                    pins[i]->addFanoutPin(pins[j]);
                }
                break;
            }
        }
    }

    // Read bin info
    in >> token >> BIN_WIDTH;
    in >> token >> BIN_HEIGHT;
    in >> token >> BIN_MAX_UTIL;
    // Read placement rows
    while(!in.eof())
    {
        int startX, startY, siteWidth, siteHeight, numSites;
        in >> token;
        if(token != "PlacementRows")
            break;
        in >> startX >> startY >> siteWidth >> siteHeight >> numSites;
        _placementRows.push_back({startX, startY, siteWidth, siteHeight, numSites});
    }
    // Read timing info
    in >> DISP_DELAY;
    for(size_t i = 0; i < _ffsLibList.size(); i++)
    {
        string cellName;
        double delay;
        in >> token >> cellName >> delay;
        _ffsLibMap[cellName]->qDelay = delay;
    }
    // slack
    while (!in.eof())
    {
        in >> token;
        if (token != "TimingSlack")
            break;
        string instName;
        string port;
        double slack;
        in >> instName >> port >> slack;
        _ffsMap[instName]->getPin(port)->setInitSlack(slack);
    }
    // Read power info
    if (token == "GatePower")
    {
        do
        {
            string cellName;
            double power;
            in >> cellName >> power;
            if(_ffsLibMap.find(cellName) != _ffsLibMap.end())
            {
                _ffsLibMap[cellName]->power = power;
            }
            in >> token;
        } while (!in.eof());
    }

    // Set prev and next stage pins
    for (auto ff : _ffs)
    {
        for (auto inPin : ff->getInputPins())
        {
            vector<Pin*> pinStack;
            pinStack.push_back(inPin); // for path tracing
            unordered_map<Pin*, bool> visited; // prevent revisiting the same pin
            Pin* fanin = inPin->getFaninPin();
            if (fanin != nullptr)
            {
                pinStack.push_back(fanin);
            }
            while (!pinStack.empty())
            {
                Pin* curPin = pinStack.back();
                if (curPin == nullptr || visited[curPin])
                {
                    pinStack.pop_back();
                }
                else
                {
                    visited[curPin] = true;
                    PinType curType = curPin->getType();
                    if (curType == PinType::FF_Q || curType == PinType::INPUT)
                    {
                        inPin->addPrevStagePin(curPin, pinStack);
                        curPin->addNextStagePin(inPin, pinStack);
                    }
                    else if (curType == PinType::GATE_OUT)
                    {
                        for (auto g_inpin : curPin->getCell()->getInputPins())
                        {
                            if (!visited[g_inpin])
                            {
                                visited[curPin] = false; // revisit gate outpin
                                pinStack.push_back(g_inpin);
                                break;
                            }
                        }
                    }
                    else if (curType == PinType::GATE_IN)
                    {
                        Pin* g_fanin = curPin->getFaninPin();
                        pinStack.push_back(g_fanin);
                    }
                    else if(curType == PinType::FF_D)
                    {
                        break;
                    }
                    else
                    {
                        std::cerr << "Error: Unexpected fanin pin type" << endl;
                        std::cerr << "Pin: " << curPin->getCell()->getInstName() << " " << curPin->getName() << endl;
                        pinStack.pop_back();
                        continue;
                    }
                }
            }
            inPin->initArrivalTime();
        }
    }
    for (auto ff : _ffs)
    {
        for (auto inPin : ff->getInputPins())
        {
            inPin->initPathMaps();
        }
    }

    // set up ff lib costPA
    for (auto ff : _ffsLibList)
    {
        ff->costPA = BETA * ff->power + GAMMA * ff->height * ff->width;
    }
    // sort ff lib list by bits
    sort(_ffsLibList.begin(), _ffsLibList.end(), [](LibCell* a, LibCell* b) -> bool { return a->bit < b->bit; });
    // set up best costPA
    for (auto ff : _ffsLibList)
    {
        const int bits = ff->bit;
        if (_bestCostPAFFs[bits] == nullptr || ff->costPA < _bestCostPA[bits])
        {
            _bestCostPAFFs[bits] = ff;
            _bestCostPA[bits] = ff->costPA;
        }
    }

    in.close();
}

void Solver::iterativePlacement()
{
    int searchDistance = _siteMap->getSites()[0]->getHeight()*2;
    // #pragma omp parallel for num_threads(NUM_THREADS)
    for(size_t i = 0; i<_ffs.size();i++)
    {
        FF* ff = _ffs[i];
        int leftDownX = ff->getX() - searchDistance;
        int leftDownY = ff->getY() - searchDistance;
        int rightUpX = ff->getX() + searchDistance;
        int rightUpY = ff->getY() + searchDistance;
        std::vector<Site*> nearSites = _siteMap->getSitesInBlock(leftDownX, leftDownY, rightUpX, rightUpY);
        double cost_min = 0;
        int best_site = -1;
        for(size_t j = 0; j < nearSites.size(); j++)
        {   
            int original_x = ff->getX();
            int original_y = ff->getY();
            int trial_x = nearSites[j]->getX();
            int trial_y = nearSites[j]->getY();
            
            // Bins cost difference when add and remove the cell
            double binCost = _binMap->moveCell(ff, trial_x, trial_y, true);
            double slackCost = calCostMoveFF(ff, original_x, original_y, trial_x, trial_y, false);

            double cost = slackCost + binCost;

            if(cost < cost_min)
            {
                cost_min = cost;
                best_site = j;
            }
        }
        // #pragma omp critical
        if(best_site != -1)
        {
            const int original_x = ff->getX();
            const int original_y = ff->getY();
            moveCell(ff, nearSites[best_site]->getX(), nearSites[best_site]->getY());
            calCostMoveFF(ff, original_x, original_y, nearSites[best_site]->getX(), nearSites[best_site]->getY(), true);
        }
    }
}

void Solver::iterativePlacementLegal()
{
    // HYPER
    int searchDistance = _siteMap->getSites()[0]->getHeight()*2;
    for(size_t i = 0; i<_ffs.size();i++)
    {
        FF* ff = _ffs[i];
        int leftDownX = ff->getX() - searchDistance;
        int leftDownY = ff->getY() - searchDistance;
        int rightUpX = ff->getX() + searchDistance;
        int rightUpY = ff->getY() + searchDistance;
        std::vector<Site*> nearSites = _siteMap->getSitesInBlock(leftDownX, leftDownY, rightUpX, rightUpY);
        double cost_min = 0;
        int best_site = -1;

        #pragma omp parallel for num_threads(NUM_THREADS)
            for(size_t j = 0; j < nearSites.size(); j++)
            {   
                if(!placeable(ff, nearSites[j]->getX(), nearSites[j]->getY()))
                    continue;
                int original_x = ff->getX();
                int original_y = ff->getY();
                int trial_x = nearSites[j]->getX();
                int trial_y = nearSites[j]->getY();
                
                // Bins cost difference when add and remove the cell
                double binCost = _binMap->moveCell(ff, trial_x, trial_y, true);
                double slackCost = calCostMoveFF(ff, original_x, original_y, trial_x, trial_y, false);

                double cost = slackCost + binCost;
                #pragma omp critical
                {
                    if(cost < cost_min)
                    {
                        cost_min = cost;
                        best_site = j;
                    }
                }
            }
        
        if(best_site != -1)
        {
            const int original_x = ff->getX();
            const int original_y = ff->getY();
            moveCell(ff, nearSites[best_site]->getX(), nearSites[best_site]->getY());
            calCostMoveFF(ff, original_x, original_y, nearSites[best_site]->getX(), nearSites[best_site]->getY(), true);
        }
    }
}

double Solver::calCost()
{
    std::cout<<"--- Calculating cost ---\n";
    double tns = 0.0;
    double power = 0.0;
    double area = 0.0;
    int numOfBinsViolated = _binMap->getNumOverMaxUtilBins();
    for(auto ff : _ffs)
    {
        tns += ff->getTotalNegativeSlack();
        power += ff->getPower();
        area += ff->getArea();
    }
    std::cout<<"TNS: "<<tns<<"\n";
    std::cout<<"Power: "<<power<<"\n";
    std::cout<<"Area: "<<area<<"\n";
    std::cout<<"Num of bins violated: "<<numOfBinsViolated<<"\n";
    double cost = ALPHA * tns + BETA * power + GAMMA * area + LAMBDA * numOfBinsViolated;
    return cost;
}

void Solver::constructFFsCLKDomain()
{
    _ffs_clkdomains.clear();
    // domain map to index
    std::map<int, int> clkDomainMap;
    for(auto ff: _ffs)
    {   
        int clkDomain = ff->getClkDomain();
        if(clkDomainMap.find(clkDomain) == clkDomainMap.end())
        {
            clkDomainMap[clkDomain] = _ffs_clkdomains.size();
            _ffs_clkdomains.push_back(std::vector<FF*>());
            _ffs_clkdomains.back().push_back(ff);
        }else{
            _ffs_clkdomains[clkDomainMap[clkDomain]].push_back(ff);
        }
    }
}

void Solver::init_placement()
{
    _binMap = new BinMap(DIE_LOW_LEFT_X, DIE_LOW_LEFT_Y, DIE_UP_RIGHT_X, DIE_UP_RIGHT_Y, BIN_WIDTH, BIN_HEIGHT);
    // Sort placement rows ascending by startY and ascending by startX
    sort(_placementRows.begin(), _placementRows.end(), [](const PlacementRows& a, const PlacementRows& b) -> bool { 
        if(a.startY == b.startY)
            return a.startX < b.startX;
        return a.startY < b.startY;
    });
    _siteMap = new SiteMap(_placementRows);

    // place cells
    for (auto comb : _combs)
    {
        placeCell(comb);
    }
    _legalizer->generateSubRows();
    for (auto ff : _ffs)
    {
        // place on the nearest site for it may not be on site initially
        Site* nearest_site = _siteMap->getNearestSite(ff->getX(), ff->getY());
        placeCell(ff, nearest_site->getX(), nearest_site->getY());
    }
}

/*
check the cell is placeable on the site at (x,y) (on site and not overlap)
Call before placing the cell if considering overlap
*/
bool Solver::placeable(Cell* cell, int x,int y)
{
    // check the cell is on site
    if(!_siteMap->onSite(x, y))
    {
        // std::cerr << "Cell not placed on site: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell in the die
    if(x < DIE_LOW_LEFT_X || x+cell->getWidth() > DIE_UP_RIGHT_X || y < DIE_LOW_LEFT_Y || y+cell->getHeight() > DIE_UP_RIGHT_Y)
    {
        // std::cerr << "Cell not in die: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell will not overlap with other cells in the bin
    std::vector<Bin*> bins = _binMap->getBins(x, y, x+cell->getWidth(), y+cell->getHeight());
    for(auto bin: bins)
    {
        for(auto c: bin->getCells())
        {
            if(c->getInstName() == DUMB_CELL_NAME || c == cell)
                continue;
            if(isOverlap(x, y, cell, c))
            {
                return false;
            }
        }
    }
    return true;
}

/*
check the proposed cell(LibCell) is placeable on the site at (x,y) (on site and not overlap)
Call before placing the cell if considering overlap
*/
bool Solver::placeable(LibCell* libCell, int x, int y)
{
    // check the cell is on site
    if(!_siteMap->onSite(x, y))
    {
        // std::cerr << "Cell not placed on site: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell in the die
    if(x < DIE_LOW_LEFT_X || x+libCell->width > DIE_UP_RIGHT_X || y < DIE_LOW_LEFT_Y || y+libCell->height > DIE_UP_RIGHT_Y)
    {
        // std::cerr << "Cell not in die: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell will not overlap with other cells in the bin
    std::vector<Bin*> bins = _binMap->getBins(x, y, x+libCell->width, y+libCell->height);
    
    for(auto bin: bins)
    {
        for(auto cell: bin->getCells())
        {
            if(cell->getInstName() == DUMB_CELL_NAME)
            {
                continue;
            }
            if(isOverlap(x, y, libCell->width, libCell->height, cell))
            {
                return false;
            }
        }
    }
    return true;
}

/*
check the cell is placeable on the site at (x,y) (on site and not overlap)
Call before placing the cell if considering overlap
move_distance is the distance the cell need to move to avoid overlap in current bins
*/
bool Solver::placeable(Cell* cell, int x, int y, int& move_distance)
{
    // check the cell is on site
    if(!_siteMap->onSite(x, y))
    {
        // std::cerr << "Cell not placed on site: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell in the die
    if(x < DIE_LOW_LEFT_X || x+cell->getWidth() > DIE_UP_RIGHT_X || y < DIE_LOW_LEFT_Y || y+cell->getHeight() > DIE_UP_RIGHT_Y)
    {
        // std::cerr << "Cell not in die: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell will not overlap with other cells in the bin
    std::vector<Bin*> bins = _binMap->getBins(x, y, x+cell->getWidth(), y+cell->getHeight());
    std::vector<Cell*> cells;
    for(auto bin: bins)
    {
        cells.insert(cells.end(), bin->getCells().begin(), bin->getCells().end());
    }
    bool overlap = false;
    int move = 0;
    for(auto c: cells)
    {
        if(c == cell || c->getInstName() == DUMB_CELL_NAME)
        {
            continue;
        }
        if(isOverlap(x, y, cell, c))
        {
            overlap = true;
            move = std::max(move, c->getX()+c->getWidth()-x);
        }
    }
    move_distance = move;
    return !overlap;
}

/*
place the cell based on the cell's x and y
*/
void Solver::placeCell(Cell* cell)
{
    _binMap->addCell(cell);
    _siteMap->place(cell);
}

/*
place the cell based on the x and y
*/
void Solver::placeCell(Cell* cell, int x, int y)
{
    cell->setXY(x, y);
    _binMap->addCell(cell);
    _siteMap->place(cell);
}

/*
remove the cell from the binMap and siteMap
*/
void Solver::removeCell(Cell* cell)
{
    _siteMap->removeCell(cell);
    _binMap->removeCell(cell);
}

/*
move the cell to (x, y), the current cost will be updated
*/
void Solver::moveCell(Cell* cell, int x, int y)
{
    if(cell->getCellType() == CellType::COMB) {
        std::cerr << "Error: moveCell for comb cell" << std::endl;
        exit(1);
    }
    removeCell(cell);
    cell->setXY(x, y);
    placeCell(cell);
}

double Solver::calCostMoveD(Pin* movedDPin, int sourceX, int sourceY, int targetX, int targetY, bool update)
{
    if (sourceX == targetX && sourceY == targetY)
    {
        return 0;
    }
    Pin* faninPin = movedDPin->getFaninPin();
    if (faninPin == nullptr)
    {
        return 0;
    }
    if (faninPin->getCell() == movedDPin->getCell())
    {
        return 0;
    }
    const int faninpin_x = faninPin->getGlobalX();
    const int faninpin_y = faninPin->getGlobalY();
    const int diff_dist = abs(targetX - faninpin_x) + abs(targetY - faninpin_y) - abs(sourceX - faninpin_x) - abs(sourceY - faninpin_y);
    const double old_slack = movedDPin->getSlack();
    const double new_slack = old_slack - DISP_DELAY * diff_dist;
    double diff_cost =  calDiffCost(old_slack, new_slack);
    if (update)
    {
        movedDPin->modArrivalTime(DISP_DELAY * diff_dist);
        movedDPin->setSlack(new_slack);
        _currCost += diff_cost;
    }
    return diff_cost;
}

double Solver::calCostMoveQ(Pin* movedQPin, int sourceX, int sourceY, int targetX, int targetY, bool update)
{
    if (sourceX == targetX && sourceY == targetY)
    {
        return 0;
    }
    double diff_cost = 0;
    for (auto nextStagePin : movedQPin->getNextStagePins())
    {
        const bool isLoopback = nextStagePin->getCell() != nullptr && nextStagePin->getCell() == movedQPin->getCell() && nextStagePin->getFaninPin() == movedQPin;
        if (!isLoopback && nextStagePin->getType() == PinType::FF_D)
        {
            const double next_old_slack = nextStagePin->getSlack();
            const double next_new_slack = nextStagePin->calSlack(movedQPin, sourceX, sourceY, targetX, targetY, update);
            diff_cost += calDiffCost(next_old_slack, next_new_slack);
        }
    }
    if (update)
    {
        _currCost += diff_cost;
    }
    return diff_cost;
}

double Solver::calCostChangeQDelay(Pin* changedQPin, double diffQDelay, bool update)
{
    double diff_cost = 0;
    for (auto nextStagePin : changedQPin->getNextStagePins())
    {
        if (nextStagePin->getType() == PinType::FF_D)
        {
            const double next_old_slack = nextStagePin->getSlack();
            const double next_new_slack = nextStagePin->calSlackQ(changedQPin, diffQDelay, update);
            diff_cost += calDiffCost(next_old_slack, next_new_slack);
        }
    }
    if (update)
    {
        _currCost += diff_cost;
    }
    return diff_cost;
}

/*
Update current cost after moving the ff
*/
double Solver::calCostMoveFF(FF* movedFF, int sourceX, int sourceY, int targetX, int targetY, bool update)
{
    if (sourceX == targetX && sourceY == targetY)
    {
        return 0;
    }
    double diff_cost = 0;
    for (auto inPin : movedFF->getInputPins())
    {
        diff_cost += calCostMoveD(inPin, sourceX + inPin->getX(), sourceY + inPin->getY(), targetX + inPin->getX(), targetY + inPin->getY(), update);
    }
    for (auto outPin : movedFF->getOutputPins())
    {
        diff_cost += calCostMoveQ(outPin, sourceX + outPin->getX(), sourceY + outPin->getY(), targetX + outPin->getX(), targetY + outPin->getY(), update);
    }
    return diff_cost;
}

double Solver::calCostBankFF(FF* ff1, FF* ff2, LibCell* targetFF, int targetX, int targetY, bool update)
{
    double diff_cost = 0;
    diff_cost += (targetFF->power - ff1->getPower() - ff2->getPower()) * BETA;
    diff_cost += (targetFF->width * targetFF->height - ff1->getArea() - ff2->getArea()) * GAMMA;
    if (update)
    {
        _currCost += diff_cost;
    }
    const int ff1_bit = ff1->getBit();
    const int ff2_bit = ff2->getBit();
    // D pin
    for (int i = 0; i < ff1_bit+ff2_bit; i++)
    {
        int op_idx = (i < ff1_bit) ? i : i - ff1_bit;
        FF* workingFF = (i < ff1_bit) ? ff1 : ff2;
        Pin* inPin = workingFF->getInputPins()[op_idx];
        Pin* mapInPin = targetFF->inputPins[i];
        Pin* faninPin = inPin->getFaninPin();
        if (faninPin->getType() != PinType::INPUT && (faninPin->getCell() == ff1 || faninPin->getCell() == ff2))
        {
            int fanin_ff_pin_idx = 0;
            for (auto p : faninPin->getCell()->getOutputPins())
            {
                if (p == faninPin) break;
                fanin_ff_pin_idx++;
            }
            if (faninPin->getCell() == ff2)
            {
                fanin_ff_pin_idx += ff1_bit;
            }
            const int old_fanin_x = faninPin->getGlobalX();
            const int old_fanin_y = faninPin->getGlobalY();
            const int new_fanin_x = targetX + targetFF->outputPins[fanin_ff_pin_idx]->getX();
            const int new_fanin_y = targetY + targetFF->outputPins[fanin_ff_pin_idx]->getY();
            const int diff_dist = abs(targetX+mapInPin->getX()-new_fanin_x) + abs(targetY+mapInPin->getY()-new_fanin_y) - abs(inPin->getGlobalX()-old_fanin_x) - abs(inPin->getGlobalY()-old_fanin_y);
            const double old_slack = inPin->getSlack();
            const double new_slack = old_slack - DISP_DELAY * diff_dist;
            const double d_cost = calDiffCost(old_slack, new_slack);
            if (update)
            {
                inPin->modArrivalTime(DISP_DELAY * diff_dist);
                inPin->setSlack(new_slack);
                _currCost += d_cost;
            }
            diff_cost += d_cost;
        }
        else
        {
            diff_cost += calCostMoveD(inPin, inPin->getGlobalX(), inPin->getGlobalY(), targetX + mapInPin->getX(), targetY + mapInPin->getY(), update);
        }
    }
    // Q pin
    for (int i = 0; i < ff1_bit+ff2_bit; i++)
    {
        int op_idx = (i < ff1_bit) ? i : i - ff1_bit;
        FF* workingFF = (i < ff1_bit) ? ff1 : ff2;
        Pin* outPin = workingFF->getOutputPins()[op_idx];
        Pin* mapOutPin = targetFF->outputPins[i];
        diff_cost += calCostChangeQDelay(outPin, targetFF->qDelay - workingFF->getQDelay(), update);
        for (auto nextStagePin : outPin->getNextStagePins())
        {
            if (nextStagePin->getType() == PinType::FF_D)
            {
                Pin* faninPin = nextStagePin->getFaninPin();
                if ((nextStagePin->getCell() == ff1 || nextStagePin->getCell() == ff2) && faninPin->getCell() == workingFF)
                {
                    // already considered in the D pin
                    continue;
                }
                else
                {
                    const double next_old_slack = nextStagePin->getSlack();
                    const double next_new_slack = nextStagePin->calSlack(outPin, outPin->getGlobalX(), outPin->getGlobalY(), targetX + mapOutPin->getX(), targetY + mapOutPin->getY(), update);
                    const double d_cost = calDiffCost(next_old_slack, next_new_slack);
                    if (update)
                    {
                        _currCost += d_cost;
                    }
                    diff_cost += d_cost;
                }
            }
        }
    }
    return diff_cost;
}

double Solver::calCostDebankFF(FF* ff, LibCell* targetFF, std::vector<int>& targetX, std::vector<int>& targetY, bool update)
{
    std::vector<std::pair<Pin*, Pin*>> dqPairs = ff->getDQpairs();
    const int x = ff->getX();
    const int y = ff->getY();
    
    double areaDiff = GAMMA*(targetFF->width * targetFF->height * ff->getBit() - ff->getArea());
    double powerDiff = BETA*(targetFF->power * ff->getBit() - ff->getPower());

    if(update)
    {
        _currCost += areaDiff + powerDiff;
    }

    double slackDiff = 0;
    const double diffQDelay = targetFF->qDelay - ff->getQDelay();

    for (size_t i=0;i<dqPairs.size();i++)
    {
        std::pair<Pin*, Pin*> dq = dqPairs[i];
        Pin *d = dq.first, *q = dq.second;

        const int original_dx = x + d->getX();
        const int original_dy = y + d->getY();
        const int original_qx = x + q->getX();
        const int original_qy = y + q->getY();
        // targetFF is always 1-bit
        const int target_dx = targetX[i] + targetFF->inputPins[0]->getX();
        const int target_dy = targetY[i] + targetFF->inputPins[0]->getY();
        const int target_qx = targetX[i] + targetFF->outputPins[0]->getX();
        const int target_qy = targetY[i] + targetFF->outputPins[0]->getY();

        slackDiff += calCostMoveD(d, original_dx, original_dy, target_dx, target_dy, update);
        slackDiff += calCostMoveQ(q, original_qx, original_qy, target_qx, target_qy, update);
        slackDiff += calCostChangeQDelay(q, diffQDelay, update);
        if (d->getFaninPin() == q)
        {
            // loop
            const double old_slack = d->getSlack();
            const double diff_dist = abs(target_dx - target_qx) + abs(target_dx - target_qy) - abs(original_dx - original_qx) - abs(original_dy - original_qy);
            const double new_slack = old_slack - DISP_DELAY * diff_dist;
            if(update)
            {
                d->modArrivalTime(DISP_DELAY * diff_dist);
                d->setSlack(new_slack);
                _currCost += calDiffCost(old_slack, new_slack);
            }
            slackDiff += calDiffCost(old_slack, new_slack);
        }
    }
    double diff_cost = areaDiff + powerDiff + slackDiff;

    return diff_cost;
}

void Solver::resetSlack(bool check)
{
    for (auto ff : _ffs)
    {
        for (auto inPin : ff->getInputPins())
        {
            inPin->resetSlack(check);
        }
    }
}

/*
add FF to _ffs and _ffsMap
*/
void Solver::addFF(FF* ff)
{
    _ffs.push_back(ff);
    _ffsMap[ff->getInstName()] = ff;
}

/*
delete FF from _ffs and _ffsMap(Complelely delete the FF)
*/
void Solver::deleteFF(FF* ff)
{
    _ffs.erase(std::remove(_ffs.begin(), _ffs.end(), ff), _ffs.end());
    _ffsMap.erase(ff->getInstName());
    delete ff;
}

void Solver::bankFFs(FF* ff1, FF* ff2, LibCell* targetFF, int x, int y)
{
    if (ff1->getClkDomain() != ff2->getClkDomain())
    {
        std::cerr << "Error: Banking FFs in different clk domains" << std::endl;
        std::cerr << "FF1: " << ff1->getInstName() << " clk domain: " << ff1->getClkDomain() << std::endl;
        std::cerr << "FF2: " << ff2->getInstName() << " clk domain: " << ff2->getClkDomain() << std::endl;
        exit(1);
    }
    removeCell(ff1);
    removeCell(ff2);
    // get the DQ pairs and clk pin
    std::vector<std::pair<Pin*, Pin*>> dqPairs1 = ff1->getDQpairs();
    std::vector<std::pair<Pin*, Pin*>> dqPairs2 = ff2->getDQpairs();
    std::vector<std::pair<Pin*, Pin*>> dqPairs;
    dqPairs.insert(dqPairs.end(), dqPairs1.begin(), dqPairs1.end());
    dqPairs.insert(dqPairs.end(), dqPairs2.begin(), dqPairs2.end());
    std::vector<Pin*> clkPins;
    clkPins.push_back(ff1->getClkPin());
    clkPins.push_back(ff2->getClkPin());
    // place the banked FF
    calCostBankFF(ff1, ff2, targetFF, x, y, true);
    FF* bankedFF = new FF(x, y, makeUniqueName(), targetFF, dqPairs, clkPins);
    bankedFF->setClkDomain(ff1->getClkDomain());
    addFF(bankedFF);
    placeCell(bankedFF);
    // free up old ffs
    deleteFF(ff1);
    deleteFF(ff2);
}

std::string Solver::makeUniqueName()
{
    const std::string newName = "AP" + std::to_string(uniqueNameCounter++);
    return newName;
}

void Solver::debankAll()
{
    // TODO: speed up
    std::vector<FF*> debankedFFs;
    std::vector<LibCell*> oneBitFFs;
    for (auto ff : _ffsLibList)
    {
        if(ff->bit == 1)
        {
            oneBitFFs.push_back(ff);
        }
    }

    for (auto ff : _ffs)
    {
        removeCell(ff);

        std::vector<std::pair<Pin*, Pin*>> dqPairs = ff->getDQpairs();
        Pin* clkPin = ff->getClkPin();
        const int x = ff->getX();
        const int y = ff->getY();
        const int clkDomain = ff->getClkDomain();

        double minCost = 0;
        LibCell* bestFF = ff->getLibCell();
        std::vector<int> bestX = {x};
        std::vector<int> bestY = {y};

        #pragma omp parallel for num_threads(NUM_THREADS)
            for(size_t i = 0; i < oneBitFFs.size(); i++)
            // for(auto oneBitFF: oneBitFFs)
            {
                LibCell* oneBitFF = oneBitFFs[i];
                std::vector<int> target_X, target_Y;
                // HYPER
                int searchDistance = ff->getWidth();
                
                int leftDownX = ff->getX() - searchDistance;
                int leftDownY = ff->getY() - searchDistance;
                int rightUpX = ff->getX() + searchDistance;
                int rightUpY = ff->getY() + searchDistance;
                std::vector<Site*> nearSites = _siteMap->getSitesInBlock(leftDownX, leftDownY, rightUpX, rightUpY);
                // sort the sites by distance
                std::sort(nearSites.begin(), nearSites.end(), [ff](Site* a, Site* b) -> bool {
                    return abs(a->getX()-ff->getX()) + abs(a->getY()-ff->getY()) < abs(b->getX()-ff->getX()) + abs(b->getY()-ff->getY());
                });

                int found = 0;
                std::vector<Cell*> candidates;

                for(size_t j = 0; j < nearSites.size(); j++)
                {   
                    if(!placeable(oneBitFF, nearSites[j]->getX(), nearSites[j]->getY()))
                        continue;
                    // check if overlap with other candidate FF
                    bool overlap = false;
                    for(auto candidate: candidates)
                    {
                        if(isOverlap(nearSites[j]->getX(), nearSites[j]->getY(), oneBitFF->width, oneBitFF->height, candidate))
                        {
                            overlap = true;
                            break;
                        }
                    }
                    if(overlap)
                        continue;
                    found++;
                    target_X.push_back(nearSites[j]->getX());
                    target_Y.push_back(nearSites[j]->getY());
                    candidates.push_back(new Cell(nearSites[j]->getX(), nearSites[j]->getY(), DUMB_CELL_NAME, oneBitFF));
                    if(found == ff->getBit())
                        break;
                }
                
                for(auto candidate: candidates)
                {
                    if(candidate != nullptr)
                        delete candidate;
                }

                if(found < ff->getBit())
                    continue;

                double costDiff = calCostDebankFF(ff, oneBitFF, target_X, target_Y, false);

                #pragma omp critical
                {
                    if (costDiff < minCost)
                    {
                        minCost = costDiff;
                        bestX = target_X;
                        bestY = target_Y;
                        bestFF = oneBitFF;
                    }
                }
            }

        if(minCost == 0)
        {
            // no better FF found
            FF* cloneFF = new FF(x, y, makeUniqueName(), bestFF, dqPairs, clkPin);
            cloneFF->setClkDomain(clkDomain);
            debankedFFs.push_back(cloneFF);
            placeCell(cloneFF);
            delete clkPin;
            continue;
        }

        calCostDebankFF(ff, bestFF, bestX, bestY, true);

        for(size_t i=0;i<dqPairs.size();i++)
        {
            FF* newFF = new FF(bestX[i], bestY[i], makeUniqueName(), bestFF, dqPairs[i], clkPin);
            newFF->setClkDomain(clkDomain);
            debankedFFs.push_back(newFF);
            placeCell(newFF);
        }
        delete clkPin;
    }

    for(size_t i=0;i<_ffs.size();i++)
    {
        delete _ffs[i];
    }
    _ffs.clear();
    _ffsMap.clear();
    for (auto ff : debankedFFs)
    {
        addFF(ff);
    }
}
/*
Calculate the cost difference given the old and new slack, the return value should be "added" to the current cost
*/
double Solver::calDiffCost(double oldSlack, double newSlack)
{
    double diff_neg_slack = 0; // difference of negative slack
    if (oldSlack <= 0 && newSlack <= 0)
    {
        diff_neg_slack = oldSlack - newSlack;
    }
    else if (oldSlack <= 0)
    {
        diff_neg_slack = oldSlack;
    }
    else if (newSlack <= 0)
    {
        diff_neg_slack = -newSlack;
    }
    return ALPHA * diff_neg_slack;
}

void Solver::solve()
{
    bool calTime = true;
    auto start = std::chrono::high_resolution_clock::now();
    auto start_solve_time = start;
    auto end = std::chrono::high_resolution_clock::now();

    std::cout<<"Alpha: "<<ALPHA<<" Beta: "<<BETA<<" Gamma: "<<GAMMA<<" Lambda: "<<LAMBDA<<"\n";

    std::cout << "\nStart initial placement...\n";
    init_placement();
    _currCost = calCost();
    _initCost = _currCost;
    std::cout << "==> Initial cost: " << _initCost << "\n";

    if(calTime)
    {
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        _stateTimes.push_back(elapsed.count());
        std::cout << "Initialization time: " << elapsed.count() << "s" << std::endl;
        start = std::chrono::high_resolution_clock::now();
    }

    bool legal = check();
    std::cout << "Legal: " << legal << "\n";
    if(!legal)
    {
        _legalizer->legalize();
    }
    saveState("Initial");

    std::cout << "\nStart to debank...\n";
    debankAll();
    _currCost = calCost();
    std::cout << "==> Cost after debanking: " << _currCost << "\n";

    if(calTime)
    {
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        _stateTimes.push_back(elapsed.count());
        std::cout << "Debanking time: " << elapsed.count() << "s" << std::endl;
        start = std::chrono::high_resolution_clock::now();
    }

    legal = check();
    std::cout << "Legal: " << legal << "\n";
    if(!legal)
    {
        _legalizer->legalize();
    }
    saveState("Debank");

    std::cout<<"\nStart to force directed placement...\n";
    iterativePlacementLegal();
    _currCost = calCost();
    std::cout << "==> Cost after force directed placement: " << _currCost << "\n";

    if(calTime)
    {
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        _stateTimes.push_back(elapsed.count());
        std::cout << "Force directed placement time: " << elapsed.count() << "s" << std::endl;
        start = std::chrono::high_resolution_clock::now();
    }

    legal = check();
    std::cout << "Legal: " << legal << "\n";
    if(!legal)
    {
        _legalizer->legalize();
    }
    saveState("ForceDirected");

    std::cout << "\nStart clustering and banking...\n";
    size_t prev_ffs_size;
    std::cout << "Init FFs size: " << _ffs.size() << "\n";
    do
    {
        constructFFsCLKDomain();
        prev_ffs_size = _ffs.size();
        for(size_t i = 0; i < _ffs_clkdomains.size(); i++)
        {
            std::vector<std::vector<FF*>> cluster;
            if (_ffs_clkdomains[i].size() > MAX_CLUSTER_SIZE)
            {
                cluster = clusteringFFs(i);
            }
            else
            {
                cluster.push_back(_ffs_clkdomains[i]);
            }
            greedyBanking(cluster);
        }
        std::cout << "FFs size after greedy banking: " << _ffs.size() << "\n";
    } while (prev_ffs_size != _ffs.size());
    _currCost = calCost();
    std::cout << "==> Cost after clustering and banking: " << _currCost << "\n";

    if(calTime)
    {
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        _stateTimes.push_back(elapsed.count());
        std::cout << "Clustering and banking time: " << elapsed.count() << "s" << std::endl;
        start = std::chrono::high_resolution_clock::now();
    }

    legal = check();
    std::cout << "Legal: " << legal << "\n";
    if(!legal)
    {
        _legalizer->legalize();
    }
    saveState("Banking");

    std::cout << "\nStart to force directed placement (second)...\n";
    iterativePlacementLegal();
    _currCost = calCost();
    std::cout << "==> Cost after force directed placement (second): " << _currCost << "\n";
    
    if(calTime)
    {
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        _stateTimes.push_back(elapsed.count());
        std::cout << "Force directed placement (second) time: " << elapsed.count() << "s" << std::endl;
        start = std::chrono::high_resolution_clock::now();
    }

    legal = check();
    std::cout << "Legal: " << legal << "\n";
    if(!legal)
    {
        _legalizer->legalize();
    }
    saveState("ForceDirected2");

    std::cout << "\nStart clustering and banking...\n";
    std::cout << "Init FFs size: " << _ffs.size() << "\n";
    do
    {
        constructFFsCLKDomain();
        prev_ffs_size = _ffs.size();
        for(size_t i = 0; i < _ffs_clkdomains.size(); i++)
        {
            std::vector<std::vector<FF*>> cluster;
            if (_ffs_clkdomains[i].size() > MAX_CLUSTER_SIZE)
            {
                cluster = clusteringFFs(i);
            }
            else
            {
                cluster.push_back(_ffs_clkdomains[i]);
            }
            greedyBanking(cluster);
        }
        std::cout << "FFs size after greedy banking: " << _ffs.size() << "\n";
    } while (prev_ffs_size != _ffs.size());
    _currCost = calCost();
    std::cout << "==> Cost after clustering and banking: " << _currCost << "\n";

    if(calTime)
    {
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        _stateTimes.push_back(elapsed.count());
        std::cout << "Clustering and banking time: " << elapsed.count() << "s" << std::endl;
        start = std::chrono::high_resolution_clock::now();
    }

    legal = check();
    std::cout << "Legal: " << legal << "\n";
    if(!legal)
    {
        _legalizer->legalize();
    }
    saveState("Banking2");

    std::cout << "\nStart to force directed placement (third)...\n";
    iterativePlacementLegal();
    _currCost = calCost();
    std::cout << "==> Cost after force directed placement (third): " << _currCost << "\n";
    
    if(calTime)
    {
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        _stateTimes.push_back(elapsed.count());
        std::cout << "Force directed placement (third) time: " << elapsed.count() << "s" << std::endl;
        start = std::chrono::high_resolution_clock::now();
    }

    if (calTime)
    {
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start_solve_time;
        _stateTimes.push_back(elapsed.count());
        std::cout << "Total solve time: " << elapsed.count() << "s" << std::endl;
    }

    legal = check();
    std::cout << "Legal: " << legal << "\n";
    if(!legal)
    {
        _legalizer->legalize();
    }
    saveState("ForceDirected3");

    std::cout << "\nCost after solving: " << _currCost << "\n";
    std::cout << "Cost difference: " << _currCost - _initCost << "\n";
    std::cout << "Cost difference percentage: " << (_currCost - _initCost) / _initCost * 100 << "%\n";
}

/*
Check for FFs in Die, FFs on Site, and Overlap
*/
bool Solver::check()
{
    std::cout << "============= Start checking =============" << std::endl;
    std::cout << "Check FF in Die" << std::endl;
    bool inDie = checkFFInDie();
    if(!inDie)
    {
        std::cerr << "============== Error ==============" << std::endl;    
        std::cerr << "FF not placed in Die" << std::endl;
    }
    std::cout << "Check FF on Site" << std::endl;
    bool onSite = checkFFOnSite();
    if(!onSite)
    {
        std::cerr << "============== Error ==============" << std::endl;    
        std::cerr << "FF not placed on Site" << std::endl;
    }
    std::cout << "Check Overlap" << std::endl;
    bool overlap = checkOverlap();
    if(overlap)
    {
        std::cerr << "============== Error ==============" << std::endl;    
        std::cerr << "Overlap" << std::endl;
    }

    if(inDie && onSite && !overlap)
    {
        std::cout << "================= Success ================" << std::endl;
        return true;
    }else{
        return false;
    }
}

/*
if any FF is not placed on site, return false
*/
bool Solver::checkFFOnSite()
{
    bool onSite = true;
    for(auto ff: _ffs)
    {
        if(!_siteMap->onSite(ff->getX(), ff->getY()))
        {
            onSite = false;
            std::cerr << "FF not placed on site: " << ff->getInstName() << std::endl;
        }
    }
    return onSite;
}

/*
if any FF is not placed or placed out of Die, return false
*/
bool Solver::checkFFInDie()
{
    bool inDie = true;
    for(auto ff: _ffs)
    {
        if(ff->getX()<DIE_LOW_LEFT_X || ff->getX()+ff->getWidth()>DIE_UP_RIGHT_X || ff->getY()<DIE_LOW_LEFT_Y || ff->getY()+ff->getHeight()>DIE_UP_RIGHT_Y){
            inDie = false;
            std::cerr << "FF not placed in Die: " << ff->getInstName() << std::endl;
        }
    }
    return inDie;
}

/*
if any cells(Combs and FFs) overlap, return true
*/
bool Solver::checkOverlap()
{
    for (auto bin : _binMap->getBins())
    {
        for (size_t i = 0; i < bin->getCells().size(); i++)
        {
            for (size_t j = i + 1; j < bin->getCells().size(); j++)
            {
                Cell* cell1 = bin->getCells()[i];
                Cell* cell2 = bin->getCells()[j];
                if (isOverlap(cell1->getX(), cell1->getY(), cell1->getWidth(), cell1->getHeight(), cell2))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

/*
if cell1(x,y) and cell2 overlap, return true
*/
inline bool Solver::isOverlap(int x1, int y1, Cell* cell1, Cell* cell2)
{
    return x1 < cell2->getX() + cell2->getWidth() &&
           x1 + cell1->getWidth() > cell2->getX() &&
           y1 < cell2->getY() + cell2->getHeight() &&
           y1 + cell1->getHeight() > cell2->getY();
}

/*
if cell1(x1,y1,w1,h1) and cell2 overlap, return true
*/
inline bool Solver::isOverlap(int x1, int y1, int w1, int h1, Cell* cell2)
{
    return x1 < cell2->getX() + cell2->getWidth() &&
           x1 + w1 > cell2->getX() &&
           y1 < cell2->getY() + cell2->getHeight() &&
           y1 + h1 > cell2->getY();
}

std::vector<int> Solver::regionQuery(std::vector<FF*> FFs, size_t idx, int eps)
{
    std::vector<int> neighbors;
    neighbors.reserve(FFs.size());
    for(size_t i = 0; i < FFs.size(); i++)
    {
        if(i == idx)
            continue;
        // Manhattan distance
        int dist = abs(FFs[i]->getX() - FFs[idx]->getX()) + abs(FFs[i]->getY() - FFs[idx]->getY());
        if(dist <= eps)
        {
            neighbors.push_back(i);
        }
    }
    std::sort(neighbors.begin(), neighbors.end(), [&](int a, int b) {
        int dist_a = abs(FFs[a]->getX() - FFs[idx]->getX()) + abs(FFs[a]->getY() - FFs[idx]->getY());
        int dist_b = abs(FFs[b]->getX() - FFs[idx]->getX()) + abs(FFs[b]->getY() - FFs[idx]->getY());
        return dist_a < dist_b;
    });
    return neighbors;
}

std::vector<std::vector<FF*>> Solver::clusteringFFs(size_t clkdomain_idx)
{
    if(clkdomain_idx >= _ffs_clkdomains.size())
    {
        std::cerr << "Invalid clk domain index" << std::endl;
        return std::vector<std::vector<FF*>>();
    }
    std::vector<FF*> FFs = _ffs_clkdomains[clkdomain_idx];
    std::vector<std::vector<FF*>> clusters;
    std::vector<bool> visited(FFs.size(), false);

    for(size_t i = 0; i < FFs.size(); i++)
    {
        if(visited[i])
            continue;
        visited[i] = true;

        std::vector<FF*> cluster;
        cluster.push_back(FFs[i]);
        // HYPER
        std::vector<int> neighbors = regionQuery(FFs, i, (DIE_UP_RIGHT_X - DIE_LOW_LEFT_X) / 100);
        if(neighbors.size() == 0)
        {
            clusters.push_back(cluster);
            continue;
        }
        for(size_t j = 0; j < neighbors.size(); j++)
        {
            int idx = neighbors[j];
            if (visited[idx])
                continue;
            
            visited[idx] = true;
            cluster.push_back(FFs[idx]);
            if(cluster.size() >= MAX_CLUSTER_SIZE)
                break;
        }
        clusters.push_back(cluster);
    }

    return clusters;
}

double Solver::cal_banking_gain(FF* ff1, FF* ff2, LibCell* targetFF, int& result_x, int& result_y)
{
    if(ff1->getBit()+ff2->getBit()!=targetFF->bit){
        std::cout << "Invalid target FF" << std::endl;
        return -INT_MAX;
    }
    
    double remove_gain = 0;
    remove_gain -= _binMap->removeCell(ff1,true);
    remove_gain -= _binMap->removeCell(ff2,true);
    
    const std::string ff1_name = ff1->getInstName();
    const std::string ff2_name = ff2->getInstName();
    ff1->setInstName(DUMB_CELL_NAME);
    ff2->setInstName(DUMB_CELL_NAME);

    int leftDownX = std::min(ff1->getX(), ff2->getX());
    int leftDownY = std::min(ff1->getY(), ff2->getY());
    int rightUpX = std::max(ff1->getX() + ff1->getWidth(), ff2->getX() + ff2->getWidth());
    int rightUpY = std::max(ff1->getY() + ff1->getHeight(), ff2->getY() + ff2->getHeight());
    double min_gain = -INFINITY;

    // set candidates
    std::vector<std::pair<int, int>> candidates;
    // HYPER
    const int div = 4;
    const double x_stride = double(rightUpX - leftDownX) / div;
    const double y_stride = double(rightUpY - leftDownY) / div;
    for(int i = 0; i < div; i++)
    {
        for(int j = 0; j < div; j++)
        {
            double x = leftDownX + i * x_stride;
            double y = leftDownY + j * y_stride;
            candidates.push_back(std::make_pair(int(x), int(y)));
        }
    }

    #pragma omp parallel for num_threads(NUM_THREADS)
        for(size_t i=0;i<candidates.size();i++)
        {
            Site* targetSite = _siteMap->getNearestSite(candidates[i].first, candidates[i].second);
            const int target_x = targetSite->getX();
            const int target_y = targetSite->getY();

            if(!placeable(targetFF, target_x, target_y))
                continue;
            
            double gain = -calCostBankFF(ff1, ff2, targetFF, target_x, target_y, false);
            gain -= _binMap->trialLibCell(targetFF, target_x, target_y);

            #pragma omp critical
            {
                if(gain > min_gain)
                {
                    min_gain = gain;
                    result_x = target_x;
                    result_y = target_y;
                }
            }
        }

    ff1->setInstName(ff1_name);
    ff2->setInstName(ff2_name);
    
    return min_gain + remove_gain;
}

void Solver::greedyBanking(std::vector<std::vector<FF*>> clusters)
{
    std::vector<LibCell*> targetFFs;
    for(auto ff : _ffsLibList)
    {
        if(ff->bit > 1)
        {
            targetFFs.push_back(ff);
        }
    }

    for(auto cluster : clusters)
    {
        if(cluster.size() < 2)
            continue;
        // prune pairs
        std::vector<std::pair<FF*, FF*>> pairs;
        std::vector<std::pair<int, double>> pair_scores;
        int pair_count = 0;
        #pragma omp parallel for num_threads(NUM_THREADS)
            for (size_t i = 0; i < cluster.size(); i++)
            {
                for (size_t j = i + 1; j < cluster.size(); j++)
                {
                    const int bit = cluster[i]->getBit() + cluster[j]->getBit();
                    if (_bestCostPAFFs[bit] == nullptr)
                    {
                        continue;
                    }
                    double score = cluster[i]->getCostPA() + cluster[j]->getCostPA() - _bestCostPA[bit];
                    if (score < 0)
                    {
                        continue;
                    }
                    double dist = std::abs(cluster[i]->getX() - cluster[j]->getX()) + std::abs(cluster[i]->getY() - cluster[j]->getY());
                    int pin_count = cluster[i]->getNSPinCount() + cluster[j]->getNSPinCount();
                    score -= dist * DISP_DELAY * ALPHA * pin_count / 2;
                    if (score < 0)
                    {
                        continue;
                    }
                    #pragma omp critical
                    {
                        pairs.push_back(std::make_pair(cluster[i], cluster[j]));
                        pair_scores.push_back(std::make_pair(pair_count++, score));
                    }
                }
            }
        // sort pairs
        std::sort(pair_scores.begin(), pair_scores.end(), [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
            return a.second > b.second;
        });

        std::vector<PairInfo> pair_infos;
        for (auto ps : pair_scores)
        {
            const std::pair<FF*, FF*>& p = pairs[ps.first];
            FF* ff1 = p.first;
            FF* ff2 = p.second;
            double pair_max_gain = 0.0;
            int pair_result_x = 0;
            int pair_result_y = 0;
            LibCell* pair_targetFF = nullptr;
            for(auto ff : targetFFs)
            {
                if(ff->bit == ff1->getBit() + ff2->getBit())
                {
                    int x, y;
                    double gain = cal_banking_gain(ff1, ff2, ff, x, y);
                    if (gain > pair_max_gain)
                    {
                        pair_max_gain = gain;
                        pair_result_x = x;
                        pair_result_y = y;
                        pair_targetFF = ff;
                    }
                }
            }
            if (pair_max_gain > 0)
            {
                pair_infos.push_back(PairInfo{ff1, ff2, pair_targetFF, pair_result_x, pair_result_y, pair_max_gain});
            }
        }
        std::sort(pair_infos.begin(), pair_infos.end(), [](const PairInfo& a, const PairInfo& b) {
            return a.gain > b.gain;
        });
        std::unordered_map<PairInfo*, bool> locked;
        for (auto pi : pair_infos)
        {
            locked[&pi] = false;
        }
        // std::cout << "Pair info size: " << pair_infos.size() << std::endl;
        if (!pair_infos.empty() && pair_infos[0].gain > 0)
        {
            for (size_t i = 0; i < pair_infos.size(); i++)
            {
                PairInfo& pi = pair_infos[i];
                if (locked[&pi])
                {
                    continue;
                }
                if (pi.gain > 0)
                {
                    const std::string ff1_name = pi.ff1->getInstName();
                    const std::string ff2_name = pi.ff2->getInstName();
                    pi.ff1->setInstName(DUMB_CELL_NAME);
                    pi.ff2->setInstName(DUMB_CELL_NAME);
                    bool isPlaceable = placeable(pi.targetFF, pi.targetX, pi.targetY);
                    pi.ff1->setInstName(ff1_name);
                    pi.ff2->setInstName(ff2_name);
                    if (isPlaceable)
                    {
                        for (size_t j = i + 1; j < pair_infos.size(); j++)
                        {
                            PairInfo& pi2 = pair_infos[j];
                            if (pi2.ff1 == pi.ff1 || pi2.ff1 == pi.ff2 || pi2.ff2 == pi.ff1 || pi2.ff2 == pi.ff2)
                            {
                                locked[&pi2] = true;
                            }
                        }
                        bankFFs(pi.ff1, pi.ff2, pi.targetFF, pi.targetX, pi.targetY);
                    }
                }
            }
        }
    }
}

void Solver::dump(std::vector<std::string>& vecStr) const
{
    using namespace std;
    string s;
    s = "CellInst " + std::to_string(_ffs.size()) + "\n"; vecStr.push_back(s);
    for (auto ff : _ffs)
    {
        s = "Inst " + ff->getInstName() + " " + ff->getCellName() + " " + std::to_string(ff->getX()) + " " + std::to_string(ff->getY()) + "\n";
        vecStr.push_back(s);
    }
    for (auto ff : _ffs)
    {
        for (auto pin : ff->getInputPins())
        {
            s = pin->getOriginalName() + " map " + pin->getCell()->getInstName() + "/" + pin->getName() + "\n";
            vecStr.push_back(s);
        }
        for (auto pin : ff->getOutputPins())
        {
            s = pin->getOriginalName() + " map " + pin->getCell()->getInstName() + "/" + pin->getName() + "\n";
            vecStr.push_back(s);
        }
        Pin* clkPin = ff->getClkPin();
        std::vector<std::string> clkPinNames = ff->getClkPin()->getOriginalNames();
        for(auto clkPinName : clkPinNames)
        {
            s = clkPinName + " map " + clkPin->getCell()->getInstName() + "/" + clkPin->getName() + "\n";
            vecStr.push_back(s);
        }
    }
}

void Solver::saveState(std::string stateName, bool legal)
{
    _stateNames.push_back(stateName);
    _stateCosts.push_back(_currCost);
    _stateLegal.push_back(legal);
    if (legal)
    {
        if (_bestCost == -1 || _currCost < _bestCost)
        {
            _bestCost = _currCost;
            _bestStateIdx = _stateCosts.size() - 1;
        }
    }
    std::vector<std::string> vecStr;
    dump(vecStr);
    _stateDumps.push_back(vecStr);
}

void Solver::report()
{
    std::cout << std::endl;
    std::cout << "------------------------------------------------------------------" << std::endl;
    std::cout << "----------------------------- Report -----------------------------" << std::endl;
    std::cout << "------------------------------------------------------------------" << std::endl;
    double firstCost = _stateCosts[0];
    for (size_t i = 0; i < _stateNames.size(); i++)
    {
        std::cout << std::setw(16) << std::left << _stateNames[i] << " \t\t " << std::right << std::scientific << std::setprecision(6) << _stateCosts[i];
        std::cout << "\t" << std::fixed << std::setprecision(2) << _stateCosts[i] / firstCost * 100 << "%";
        if(_stateTimes.size() > i)
        {
            std::cout << "\t" << std::fixed << std::setprecision(2) << _stateTimes[i] << "s";
        }
        if (_bestStateIdx == i)
        {
            std::cout << " *";
        }
        std::cout << std::endl;
    }
    std::cout << std::setw(16) << std::left << "Best/Total time\t\t " << std::right << std::scientific << std::setprecision(6) << _stateCosts[_bestStateIdx];
    std::cout << "\t" << std::fixed << std::setprecision(2) << _stateCosts[_bestStateIdx] / firstCost * 100 << "%";
    if(_stateTimes.size() > 0)
    {
        std::cout << "\t" << std::fixed << std::setprecision(2) << _stateTimes.back() << "s";
    }
    std::cout << "\n";
    std::cout << "------------------------------------------------------------------" << std::endl;
}

void Solver::dump_best(std::string filename) const
{
    using namespace std;
    ofstream out(filename);
    const vector<string>& vecStr = _stateDumps.at((_bestCost != -1) ? _bestStateIdx : _stateDumps.size() - 1);
    for (auto str : vecStr)
    {
        out << str;
    }
}