#include "Solver.h"
#include "Cell.h"
#include "Comb.h"
#include "FF.h"
#include "Net.h"
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
        delete ff;
    }
    for(auto comb : _combs)
    {
        delete comb;
    }
    for(auto net : _nets)
    {
        delete net;
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
            const int numDQpairs = bits;
            for (int i = 0; i < numDQpairs; i++)
            {
                ff->inputPins[i]->addFanoutPin(ff->outputPins[i]);
            }
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
            // set fanout of input pins to output pins
            for (auto inPin : comb->inputPins)
            {
                inPin->addFanoutPin(comb->outputPins[0]);
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
        Net* newNet = new Net(netName);
        vector<Pin*> pins;
        for(int j = 0; j < pinCount; j++)
        {
            string pinName;
            in >> token >> pinName;

            string instName = pinName.substr(0, pinName.find('/'));
            string pin;
            if(pinName.find('/') != string::npos)
                pin = pinName.substr(pinName.find('/') + 1);
            if((toLower(pin) == "clk" || toLower(pinName) == "clk") && !clknet){
                clknet = true;
                _ffs_clkdomains.push_back(vector<FF*>());
            }
            Pin* p = nullptr;
            if(_ffsMap.find(instName) != _ffsMap.end()){
                p = _ffsMap[instName]->getPin(pin);
                if(toLower(pin) == "clk" && clknet){
                    _ffs_clkdomains.back().push_back(_ffsMap[instName]);
                    _ffs_clkdomains.back().back()->setClkDomain(_ffs_clkdomains.size()-1);
                }
            }else if(_combsMap.find(instName) != _combsMap.end()){
                p = _combsMap[instName]->getPin(pin);
            }else if(_inputPinsMap.find(pinName) != _inputPinsMap.end()){
                p = _inputPinsMap[pinName];
            }else if(_outputPinsMap.find(pinName) != _outputPinsMap.end()){
                p = _outputPinsMap[pinName];
            }else{
                cerr << "Error: Pin not found: " << pinName << endl;
                exit(1);
            }
            pins.push_back(p);
            p->connect(newNet);
        }
        newNet->setPins(pins);
        _nets.push_back(newNet);
        _netsMap[netName] = newNet;
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
    size_t totalBits = 0;
    for(size_t i = 0; i < _ffs.size(); i++)
    {
        totalBits += _ffs[i]->getBit();
    }
    // slack
    for(size_t i = 0; i < totalBits; i++)
    {
        string instName;
        string port;
        double slack;
        in >> token >> instName >> port >> slack;
        _ffsMap[instName]->getPin(port)->setInitSlack(slack);
    }
    // Read power info
    for(size_t i = 0; i < _ffsLibList.size(); i++)
    {
        string cellName;
        double power;
        in >> token >> cellName >> power;
        if(_ffsLibMap.find(cellName) != _ffsLibMap.end())
        {
            _ffsLibMap[cellName]->power = power;
        }
    }
    // set power to instances

    // Set prev and next stage pins
    // TODO: check if is needed to set prev and next stage pins for INPUT and OUTPUT pins
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
                        exit(1);
                    }
                }
            }
            inPin->initArrivalTime();
        }
    }

    cout << "File parsed successfully" << endl;
    in.close();
}

void Solver::iterativePlacement()
{
    int searchDistance = _siteMap->getSites()[0]->getHeight()*2;
    #pragma omp parallel for num_threads(NUM_THREADS)
    for(size_t i = 0; i<_ffs.size();i++)
    {
        FF* ff = _ffs[i];
        if(ff->getTotalNegativeSlack() < 1e-1)
            continue;
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
            double removeCost = _binMap->removeCell(ff, true);
            double addCost = 0;
            ff->setXY(trial_x, trial_y);
            addCost = _binMap->addCell(ff, true);
            ff->setXY(original_x, original_y);        

            double slackCost = calCostMoveFF(ff, original_x, original_y, trial_x, trial_y, false);

            double cost = slackCost + removeCost + addCost;

            if(cost < cost_min)
            {
                cost_min = cost;
                best_site = j;
            }
        }
        #pragma omp critical
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
    int searchDistance = _siteMap->getSites()[0]->getHeight()*2;
    #pragma omp parallel for num_threads(NUM_THREADS)
    for(size_t i = 0; i<_ffs.size();i++)
    {
        FF* ff = _ffs[i];
        if(ff->getTotalNegativeSlack() < 1e-1)
            continue;
        int leftDownX = ff->getX() - searchDistance;
        int leftDownY = ff->getY() - searchDistance;
        int rightUpX = ff->getX() + searchDistance;
        int rightUpY = ff->getY() + searchDistance;
        std::vector<Site*> nearSites = _siteMap->getSitesInBlock(leftDownX, leftDownY, rightUpX, rightUpY);
        double cost_min = 0;
        int best_site = -1;
        for(size_t j = 0; j < nearSites.size(); j++)
        {   
            if(!placeable(ff, nearSites[j]->getX(), nearSites[j]->getY()))
                continue;
            int original_x = ff->getX();
            int original_y = ff->getY();
            int trial_x = nearSites[j]->getX();
            int trial_y = nearSites[j]->getY();
            
            // Bins cost difference when add and remove the cell
            double removeCost = _binMap->removeCell(ff, true);
            double addCost = 0;
            ff->setXY(trial_x, trial_y);
            addCost = _binMap->addCell(ff, true);
            ff->setXY(original_x, original_y);        

            double slackCost = calCostMoveFF(ff, original_x, original_y, trial_x, trial_y, false);

            double cost = slackCost + removeCost + addCost;

            if(cost < cost_min)
            {
                cost_min = cost;
                best_site = j;
            }
        }
        #pragma omp critical
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

void Solver::checkCLKDomain()
{
    constructFFsCLKDomain();
    int sum = 0;
    for(size_t i=0;i<_ffs_clkdomains.size();i++){
        sum += _ffs_clkdomains[i].size();
        std::cout<<"Sum from clk domain "<<i<<" : "<<_ffs_clkdomains[i].size()<<std::endl;
    }
    std::cout<<"Sum from CLK domain: "<<sum<<std::endl;
    std::cout<<"Sum from list: "<<_ffs.size()<<std::endl;
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
    // Sort placement rows ascending by startY
    sort(_placementRows.begin(), _placementRows.end(), [](const PlacementRows& a, const PlacementRows& b) -> bool { return a.startY < b.startY; });
    _siteMap = new SiteMap(_placementRows);

    // place cells
    for (auto ff : _ffs)
    {
        // place on the nearest site for it may not be on site initially
        Site* nearest_site = _siteMap->getNearestSite(ff->getX(), ff->getY());
        placeCell(ff, nearest_site->getX(), nearest_site->getY());
    }
    for (auto comb : _combs)
    {
        placeCell(comb);
    }

    _legalizer->generateSubRows();
}

/*
check the cell is placeable on the site(on site and not overlap)
Call before placing the cell if considering overlap
*/
bool Solver::placeable(Cell* cell)
{
    // check the cell is on site
    if(!_siteMap->onSite(cell->getX(), cell->getY()))
    {
        // std::cerr << "Cell not placed on site: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell in the die
    if(cell->getX() < DIE_LOW_LEFT_X || cell->getX()+cell->getWidth() > DIE_UP_RIGHT_X || cell->getY() < DIE_LOW_LEFT_Y || cell->getY()+cell->getHeight() > DIE_UP_RIGHT_Y)
    {
        // std::cerr << "Cell not in die: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell will not overlap with other cells in the bin
    std::vector<Bin*> bins = _binMap->getBins(cell->getX(), cell->getY(), cell->getX()+cell->getWidth(), cell->getY()+cell->getHeight());
    std::vector<Cell*> cells;
    for(auto bin: bins)
    {   
        cells.insert(cells.end(), bin->getCells().begin(), bin->getCells().end());
    }
    for(auto c: cells)
    {
        if(c == cell)
            continue;
        if(isOverlap(cell, c))
        {
            return false;
        }
    }
    return true;
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
    std::vector<Cell*> cells;
    for(auto bin: bins)
    {
        cells.insert(cells.end(), bin->getCells().begin(), bin->getCells().end());
    }
    for(auto c: cells)
    {
        if(c == cell)
            continue;
        if(isOverlap(x, y, cell, c))
        {
            return false;
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
    std::vector<Cell*> cells;
    for(auto bin: bins)
    {
        cells.insert(cells.end(), bin->getCells().begin(), bin->getCells().end());
    }
    for(auto c: cells)
    {
        if(isOverlap(x, y, libCell->width, libCell->height, c))
        {
            return false;
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
        if(c == cell)
            continue;
        if(isOverlap(x, y, cell, c))
        {
            overlap = true;
            move = std::max(move, c->getX()+c->getWidth()-x);
            if(move == 0) {
                std::cerr << "Error: move distance is 0" << std::endl;
                std::cout << c->getX() << " " << c->getWidth() << " " << x << " " << y << std::endl;
            }
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
    cell->setX(x);
    cell->setY(y);
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
    cell->setX(x);
    cell->setY(y);
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
    
    // TODO: multi-bits
    double areaDiff = GAMMA*(targetFF->width * targetFF->height - ff->getArea());
    double powerDiff = BETA*(targetFF->power - ff->getPower());

    if(update)
    {
        _currCost += areaDiff + powerDiff;
    }

    double slackDiff = 0;
    const double diffQDelay = targetFF->qDelay - ff->getQDelay();

    // if(dqPairs.size() != targetX.size() || dqPairs.size() != targetY.size())
    // {
    //     std::cerr << "Error: Debank FF target size mismatch" << std::endl;
    //     exit(1);
    // }

    for (size_t i=0;i<dqPairs.size();i++)
    {
        std::pair<Pin*, Pin*> dq = dqPairs[i];
        Pin *d = dq.first, *q = dq.second;

        const int original_dx = x + d->getX();
        const int original_dy = y + d->getY();
        const int original_qx = x + q->getX();
        const int original_qy = y + q->getY();

        const int target_dx = targetX[i] + targetFF->inputPins[i]->getX();
        const int target_dy = targetY[i] + targetFF->inputPins[i]->getY();
        const int target_qx = targetX[i] + targetFF->outputPins[i]->getX();
        const int target_qy = targetY[i] + targetFF->outputPins[i]->getY();

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
    int target_x, target_y;
    target_x = x;
    target_y = y;
    calCostBankFF(ff1, ff2, targetFF, target_x, target_y, true);
    FF* bankedFF = new FF(target_x, target_y, makeUniqueName(), targetFF, dqPairs, clkPins);
    bankedFF->setClkDomain(ff1->getClkDomain());
    addFF(bankedFF);
    placeCell(bankedFF);
    // free up old ffs
    deleteFF(ff1);
    deleteFF(ff2);
}

std::string Solver::makeUniqueName()
{
    std::string newName = "APPLEBUSTER" + std::to_string(uniqueNameCounter++);
    return newName;
}

void Solver::debankAll()
{
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
        for(auto oneBitFF: oneBitFFs)
        {
            // check if the FF can be placed
            if(!placeable(oneBitFF, x, y))
                continue;
            std::vector<int> target_X, target_Y;
            // TODO: multi-bits
            target_X.push_back(x);
            target_Y.push_back(y);

            double costDiff = calCostDebankFF(ff, oneBitFF, target_X, target_Y, false);
            #pragma omp critical
            if (costDiff < minCost)
            {
                minCost = costDiff;
                bestX = target_X;
                bestY = target_Y;
                bestFF = oneBitFF;
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

void Solver::forceDirectedPlaceFFLock(const int ff_idx, std::vector<bool>& locked, std::vector<char>& lock_cnt, int& lock_num)
{
    if (locked[ff_idx])
    {
        return;
    }
    // calculate the force
    std::vector<double> x_list, y_list, slack_list;
    FF* ff = _ffs[ff_idx];
    // display ff's info
    for (auto inPin : ff->getInputPins())
    {
        Pin* fanin = inPin->getFaninPin();
        if(fanin == nullptr)
        {
            continue;
        }
        x_list.push_back(fanin->getGlobalX());
        y_list.push_back(fanin->getGlobalY());
        double slack_reducable = abs(inPin->getGlobalX() - fanin->getGlobalX()) + abs(inPin->getGlobalY() - fanin->getGlobalY()) + 1;
        slack_reducable *= DISP_DELAY;
        if(inPin->getSlack() < 0)
        {
            slack_list.push_back(std::min(-slack_reducable/inPin->getSlack(), 1.0));
        }else{
            slack_list.push_back(1/(inPin->getSlack()+1));
        }
    }
    for (auto outPin : ff->getOutputPins())
    {
        for (auto path : outPin->getPathToNextStagePins())
        {
            Pin* fanout = path.at(path.size()-2);
            x_list.push_back(fanout->getGlobalX());
            y_list.push_back(fanout->getGlobalY());
            Pin* nextStagePin = path.front();
            double slack_reducable = abs(outPin->getGlobalX() - fanout->getGlobalX()) + abs(outPin->getGlobalY() - fanout->getGlobalY()) + 1;
            slack_reducable *= DISP_DELAY;
            if(nextStagePin->getSlack() < 0)
            {
                slack_list.push_back(std::min(-slack_reducable/nextStagePin->getSlack(), 1.0));
            }else{
                slack_list.push_back(1/(nextStagePin->getSlack()+1));
            }
        }
    }
    if (slack_list.size() == 0)
    {
        locked[ff_idx] = true;
    }
    else
    {
        // for (auto& slack : slack_list)
        // {
        //     slack = (slack < 0) ? 20 : 1;
        //     // slack = -slack;
        //     // slack = -slack+min_slack;
        //     // slack = -slack+min_slack+1;
        //     // slack = std::exp(-slack);
        //     // slack = std::exp(-0.001*slack);
        // }
        double x_sum = 0;
        double y_sum = 0;
        double total_slack = 0;
        for (size_t i = 0; i < x_list.size(); i++)
        {
            x_sum += x_list[i] * slack_list[i];
            y_sum += y_list[i] * slack_list[i];
            total_slack += slack_list[i];
        }
        if (total_slack == 0)
        {
            std::cerr << "Error: total slack is 0" << std::endl;
            exit(1);
        }
        int x_avg = x_sum / total_slack;
        int y_avg = y_sum / total_slack;
        Site* nearest_site = _siteMap->getNearestSite(x_avg, y_avg);
        const int source_x = ff->getX();
        const int source_y = ff->getY();
        const int target_x = nearest_site->getX();
        const int target_y = nearest_site->getY();
        double dist = abs(source_x - target_x) + abs(source_y - target_y);
        moveCell(ff, target_x, target_y);
        calCostMoveFF(ff, source_x, source_y, target_x, target_y, true);
        // check if the ff should be locked
        if (dist < nearest_site->getWidth() / 2)
        {
            lock_cnt[ff_idx]++;
        }
        if (lock_cnt[ff_idx] > 1)
        {
            locked[ff_idx] = true;
            // also lock fanin and fanout
            // for (auto inPin : ff->getInputPins())
            // {
            //     Pin* fanin = inPin->getFaninPin();
            //     if(fanin == nullptr)
            //         continue;
            //     Cell* cell = fanin->getCell();
            //     if(cell == nullptr)
            //         continue;
            //     if(cell->getCellType() == CellType::FF)
            //     {
            //         FF* faninFF = static_cast<FF*>(cell);
            //         int fanin_idx = -1;
            //         for (size_t i = 0; i < _ffs.size(); i++)
            //         {
            //             if (_ffs[i] == faninFF)
            //             {
            //                 fanin_idx = i;
            //                 break;
            //             }
            //         }
            //         if (fanin_idx != -1)
            //         {
            //             locked[fanin_idx] = true;
            //         }
            //     }
            // }
            // for (auto outPin : ff->getOutputPins())
            // {
            //     for (auto path : outPin->getPathToNextStagePins())
            //     {
            //         Pin* fanout = path.at(path.size()-2);
            //         Cell* cell = fanout->getCell();
            //         if(cell == nullptr)
            //             continue;
            //         if(cell->getCellType() == CellType::FF)
            //         {
            //             FF* fanoutFF = static_cast<FF*>(cell);
            //             int fanout_idx = -1;
            //             for (size_t i = 0; i < _ffs.size(); i++)
            //             {
            //                 if (_ffs[i] == fanoutFF)
            //                 {
            //                     fanout_idx = i;
            //                     break;
            //                 }
            //             }
            //             if (fanout_idx != -1)
            //             {
            //                 locked[fanout_idx] = true;
            //             }
            //         }
            //     }
            // }
        }
    }
    if (locked[ff_idx])
    {
        lock_num++;
    }
}

void Solver::forceDirectedPlacement()
{
    bool converged = false;
    int max_iter = 1000;
    int iter = 0;
    const int numFFs = _ffs.size();
    const int lockThreshold = numFFs;
    std::cout << "Lock threshold: " << lockThreshold << std::endl;
    const int diff_lock_threshold = 5;
    const int diff_lock_count_threshold = 20;
    int diff_lock_count = 0;
    int prev_lock_num = 0;
    int lock_num = 0;
    std::vector<char> lock_cnt(numFFs, 0);
    std::vector<bool> locked(numFFs, false);
    while(!converged && iter++ < max_iter){
        for (int i = 0; i < numFFs; i++)
        {
            forceDirectedPlaceFFLock(i, locked, lock_cnt, lock_num);
        }
        std::cout << "Iteration: " << iter << ", Lock/Total: " << lock_num << "/" << numFFs << std::endl;
        if (lock_num - prev_lock_num < diff_lock_threshold)
        {
            diff_lock_count++;
        }
        else
        {
            diff_lock_count = 0;
        }
        converged = (lock_num >= lockThreshold) || (diff_lock_count > diff_lock_count_threshold);
        prev_lock_num = lock_num;
    }
}

void Solver::fineTune()
{
    std::cout << "Fine tuning..." << std::endl;
    std::cout << "Bin Grid: " << _binMap->getNumBinsX() << "x" << _binMap->getNumBinsY() << std::endl;

    int kernel_size = 5;
    int stride_x = 1;
    int stride_y = 1;
    int stride_x_width = stride_x * BIN_WIDTH;
    int stride_y_height = stride_y * BIN_HEIGHT;
    int num_x = (_binMap->getNumBinsX() - kernel_size) / stride_x + 1;
    int num_y = (_binMap->getNumBinsY() - kernel_size) / stride_y + 1;

    double cost_difference = 0;

    for(int y = 0; y < num_y; y++)
    {
        std::cout << "Fine tuning: " << y << "/" << num_y << std::endl;
        for(int x = 0; x < num_x; x++)
        {
            int cur_x = x * stride_x_width;
            int cur_y = y * stride_y_height;
            int upright_x = std::min(cur_x + kernel_size * BIN_WIDTH, DIE_UP_RIGHT_X);
            int upright_y = std::min(cur_y + kernel_size * BIN_HEIGHT, DIE_UP_RIGHT_Y);

            std::vector<Bin*> bins = _binMap->getBinsBlocks(x, y, kernel_size, kernel_size);
            bool is_over_max_util = false;
            for(size_t i = 0; i < bins.size(); i++)
            {
                if(bins[i]->isOverMaxUtil())
                {
                    is_over_max_util = true;
                    break;
                }
            }

            // continue if the bins utilization rate is less than threshold
            if(!is_over_max_util)
                continue;

            std::vector<FF*> ffs_local = _binMap->getFFsInBins(bins);
            std::vector<Site*> sites = _siteMap->getSitesInBlock(cur_x, cur_y, upright_x, upright_y);
            
            for(size_t i = 0; i < ffs_local.size(); i++)
            {
                FF* ff = ffs_local[i];
                double cost_min = 0;
                int best_site = -1;
                for(size_t j = 0; j < sites.size(); j++)
                {
                    if(!placeable(ff, sites[j]->getX(), sites[j]->getY()))
                        continue;
                    
                    int original_x = ff->getX();
                    int original_y = ff->getY();
                    int trial_x = sites[j]->getX();
                    int trial_y = sites[j]->getY();
                    
                    // Bins cost difference when add and remove the cell
                    double removeCost = _binMap->removeCell(ff, true);
                    double addCost = 0;
                    ff->setXY(trial_x, trial_y);
                    addCost = _binMap->addCell(ff, true);
                    ff->setXY(original_x, original_y);        

                    double slackCost = calCostMoveFF(ff, original_x, original_y, trial_x, trial_y, false);

                    double cost = slackCost + removeCost + addCost;

                    if(cost < cost_min)
                    {
                        cost_min = cost;
                        best_site = j;
                    }
                }
                if(best_site != -1)
                {
                    cost_difference += cost_min;
                    const int original_x = ff->getX();
                    const int original_y = ff->getY();
                    moveCell(ff, sites[best_site]->getX(), sites[best_site]->getY());
                    calCostMoveFF(ff, original_x, original_y, sites[best_site]->getX(), sites[best_site]->getY(), true);
                }
            }
        }
    }
    std::cout << "Cost difference: " << cost_difference << std::endl;
}

void Solver::solve()
{
    bool calTime = true;
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();

    std::cout<<"Alpha: "<<ALPHA<<" Beta: "<<BETA<<" Gamma: "<<GAMMA<<" Lambda: "<<LAMBDA<<"\n";

    std::cout << "\nStart initial placement...\n";
    init_placement();
    _currCost = calCost();
    _initCost = _currCost;
    std::cout << "==> Initial cost: " << _initCost << "\n";
    saveState("Initial");

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
    
    std::cout << "\nStart to debank...\n";
    debankAll();
    _currCost = calCost();
    std::cout << "==> Cost after debanking: " << _currCost << "\n";
    saveState("Debank");

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
    
    std::cout<<"\nStart to force directed placement...\n";
    iterativePlacementLegal();
    _currCost = calCost();
    std::cout << "==> Cost after force directed placement: " << _currCost << "\n";
    saveState("ForceDirected");

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

    std::cout << "\nStart clustering and banking...\n";
    size_t prev_ffs_size;
    std::cout << "Init FFs size: " << _ffs.size() << "\n";
    do
    {
        constructFFsCLKDomain();
        prev_ffs_size = _ffs.size();
        for(size_t i = 0; i < _ffs_clkdomains.size(); i++)
        {
            std::vector<std::vector<FF*>> cluster = clusteringFFs(i);
            greedyBanking(cluster);
        }
        std::cout << "FFs size after greedy banking: " << _ffs.size() << "\n";
    } while (prev_ffs_size != _ffs.size());
    _currCost = calCost();
    std::cout << "==> Cost after clustering and banking: " << _currCost << "\n";
    saveState("Banking");

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

    std::cout << "\nStart to force directed placement (second)...\n";
    iterativePlacementLegal();
    _currCost = calCost();
    std::cout << "==> Cost after force directed placement (second): " << _currCost << "\n";
    saveState("ForceDirected2");
    
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

    std::cout << "\nStart clustering and banking...\n";
    std::cout << "Init FFs size: " << _ffs.size() << "\n";
    do
    {
        constructFFsCLKDomain();
        prev_ffs_size = _ffs.size();
        for(size_t i = 0; i < _ffs_clkdomains.size(); i++)
        {
            std::vector<std::vector<FF*>> cluster = clusteringFFs(i);
            greedyBanking(cluster);
        }
        std::cout << "FFs size after greedy banking: " << _ffs.size() << "\n";
    } while (prev_ffs_size != _ffs.size());
    _currCost = calCost();
    std::cout << "==> Cost after clustering and banking: " << _currCost << "\n";
    saveState("Banking");

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

    std::cout << "\nStart to force directed placement (second)...\n";
    iterativePlacementLegal();
    _currCost = calCost();
    std::cout << "==> Cost after force directed placement (second): " << _currCost << "\n";
    saveState("ForceDirected2");
    
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
    
    // // std::cout<<"\nStart to legalize...\n";
    // // _legalizer->legalize();
    // // resetSlack();
    // // _currCost = calCost();
    // // std::cout << "==> Cost after reset slack: " << _currCost << "\n";
    // // saveState("Legalize", true);

    // // if(calTime)
    // // {
    // //     end = std::chrono::high_resolution_clock::now();
    // //     std::chrono::duration<double> elapsed = end - start;
    // //     _stateTimes.push_back(elapsed.count());
    // //     std::cout << "Legalization time: " << elapsed.count() << "s" << std::endl;
    // //     start = std::chrono::high_resolution_clock::now();
    // // }

    // std::cout<<"\nStart to force directed placement (Legal)...\n";
    // iterativePlacementLegal();
    // _currCost = calCost();
    // std::cout << "==> Cost after force directed placement (Legal): " << _currCost << "\n";
    // saveState("ForceDirectedLegal", true);

    // if(calTime)
    // {
    //     end = std::chrono::high_resolution_clock::now();
    //     std::chrono::duration<double> elapsed = end - start;
    //     _stateTimes.push_back(elapsed.count());
    //     std::cout << "Force directed placement (Legal) time: " << elapsed.count() << "s" << std::endl;
    //     start = std::chrono::high_resolution_clock::now();
    // }

    // std::cout<<"\nStart to fine tune...\n";
    // fineTune();
    // resetSlack();
    // _currCost = calCost();
    // std::cout << "==> Cost after reset slack: " << _currCost << "\n";
    // saveState("FineTune", true);

    // if(calTime)
    // {
    //     end = std::chrono::high_resolution_clock::now();
    //     std::chrono::duration<double> elapsed = end - start;
    //     _stateTimes.push_back(elapsed.count());
    //     std::cout << "Fine tuning time: " << elapsed.count() << "s" << std::endl;
    // }

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
        if(ff->getSites().size() == 0)
        {
            inDie = false;
            // std::cerr << "FF not placed: " << ff->getInstName() << std::endl;
        }else if(ff->getX()<DIE_LOW_LEFT_X || ff->getX()+ff->getWidth()>DIE_UP_RIGHT_X || ff->getY()<DIE_LOW_LEFT_Y || ff->getY()+ff->getHeight()>DIE_UP_RIGHT_Y){
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
    std::vector<Cell*> cells;
    cells.insert(cells.end(), _combs.begin(), _combs.end());
    cells.insert(cells.end(), _ffs.begin(), _ffs.end());
    bool overlap = false;
    for(size_t i = 0; i < cells.size(); i++)
    {
        // for(size_t j = i+1; j < cells.size(); j++)
        // {
        //     if(isOverlap(cells[i], cells[j]))
        //     {
        //         std::cerr << "Overlap: " << cells[i]->getInstName() << " and " << cells[j]->getInstName() << std::endl;
        //         std::cerr << "Cell1: " << cells[i]->getX() << " " << cells[i]->getY() << " " << cells[i]->getWidth() << " " << cells[i]->getHeight() << std::endl;
        //         std::cerr << "Cell2: " << cells[j]->getX() << " " << cells[j]->getY() << " " << cells[j]->getWidth() << " " << cells[j]->getHeight() << std::endl;
        //         overlap = true;
        //     }
        // }
        
        if(cells[i]->checkOverlap()){
            std::cerr << "Overlap: " << cells[i]->getInstName() << std::endl;
            overlap = true;
        }
    }
    return overlap;
}

/*
if cell1 and cell2 overlap, return true
*/
bool Solver::isOverlap(Cell* cell1, Cell* cell2)
{
    return cell1->getX() < cell2->getX() + cell2->getWidth() &&
           cell1->getX() + cell1->getWidth() > cell2->getX() &&
           cell1->getY() < cell2->getY() + cell2->getHeight() &&
           cell1->getY() + cell1->getHeight() > cell2->getY();
}

/*
if cell1(x,y) and cell2 overlap, return true
*/
bool Solver::isOverlap(int x1, int y1, Cell* cell1, Cell* cell2)
{
    return x1 < cell2->getX() + cell2->getWidth() &&
           x1 + cell1->getWidth() > cell2->getX() &&
           y1 < cell2->getY() + cell2->getHeight() &&
           y1 + cell1->getHeight() > cell2->getY();
}

/*
if cell1(x1,y1,w1,h1) and cell2 overlap, return true
*/
bool Solver::isOverlap(int x1, int y1, int w1, int h1, Cell* cell2)
{
    return x1 < cell2->getX() + cell2->getWidth() &&
           x1 + w1 > cell2->getX() &&
           y1 < cell2->getY() + cell2->getHeight() &&
           y1 + h1 > cell2->getY();
}

std::vector<int> Solver::regionQuery(std::vector<FF*> FFs, size_t idx, int eps)
{
    std::vector<int> neighbors;
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
    std::vector<int> cluster_num(FFs.size(), -1);
    size_t max_cluster_size = 50;
    // DBSCAN
    // TODO: optimize the algorithm or change to other clustering algorithm
    for(size_t i = 0; i < FFs.size(); i++)
    {
        if(visited[i])
            continue;
        visited[i] = true;

        std::vector<FF*> cluster;
        cluster.push_back(FFs[i]);
        cluster_num[i] = clusters.size();
        std::vector<int> neighbors = regionQuery(FFs, i, (DIE_UP_RIGHT_X - DIE_LOW_LEFT_X) / 100);
        if(neighbors.size() == 0)
        {
            // noise
            clusters.push_back(cluster);
            continue;
        }
        for(size_t j = 0; j < neighbors.size(); j++)
        {
            int idx = neighbors[j];
            if (visited[idx])
                continue;
            
            visited[idx] = true;
            cluster_num[idx] = clusters.size();
            cluster.push_back(FFs[idx]);
            // std::vector<int> new_neighbors = regionQuery(FFs, idx, BIN_WIDTH);
            // if(new_neighbors.size() >= 2)
            // {
            //     for(size_t k = 0; k < new_neighbors.size(); k++)
            //     {
            //         if(std::find(neighbors.begin(), neighbors.end(), new_neighbors[k]) == neighbors.end())
            //         {
            //             neighbors.push_back(new_neighbors[k]);
            //         }
            //     }
            // }
        
            // if(std::find(cluster.begin(), cluster.end(), FFs[idx]) == cluster.end())
            // {
            //     cluster.push_back(FFs[idx]);
            // }
            if(cluster.size() >= max_cluster_size)
                break;
        }
        clusters.push_back(cluster);
    }

    return clusters;
}
// TODO: find better position for banking FFs
void Solver::findForceDirectedPlacementBankingFFs(FF* ff1, FF* ff2, int& result_x, int& result_y)
{
    std::vector<double> x_list, y_list, slack_list;
    for (auto inPin : ff1->getInputPins())
    {
        Pin* fanin = inPin->getFaninPin();
        if (fanin->getCell() != ff2)
        {
            x_list.push_back(fanin->getGlobalX());
            y_list.push_back(fanin->getGlobalY());
            slack_list.push_back(inPin->getSlack());
        }
    }
    for (auto inPin : ff2->getInputPins())
    {
        Pin* fanin = inPin->getFaninPin();
        if (fanin->getCell() != ff1)
        {
            x_list.push_back(fanin->getGlobalX());
            y_list.push_back(fanin->getGlobalY());
            slack_list.push_back(inPin->getSlack());
        }
    }
    for (auto outPin : ff1->getOutputPins())
    {
        for (auto path : outPin->getPathToNextStagePins())
        {
            Pin* fanout = path.at(path.size()-2);
            if (fanout->getCell() != ff2)
            {
                x_list.push_back(fanout->getGlobalX());
                y_list.push_back(fanout->getGlobalY());
                Pin* nextStagePin = path.front();
                slack_list.push_back(nextStagePin->getSlack());
            }
        }
    }
    for (auto outPin : ff2->getOutputPins())
    {
        for (auto path : outPin->getPathToNextStagePins())
        {
            Pin* fanout = path.at(path.size()-2);
            if (fanout->getCell() != ff1)
            {
                x_list.push_back(fanout->getGlobalX());
                y_list.push_back(fanout->getGlobalY());
                Pin* nextStagePin = path.front();
                slack_list.push_back(nextStagePin->getSlack());
            }
        }
    }
    if (slack_list.size() == 0)
    {
        int x_avg = (ff1->getX() + ff2->getX()) / 2;
        int y_avg = (ff1->getY() + ff2->getY()) / 2;
        Site* nearest_site = _siteMap->getNearestSite(x_avg, y_avg);
        result_x = nearest_site->getX();
        result_y = nearest_site->getY();
    }
    else
    {
        for (auto& slack : slack_list)
        {
            slack = (slack < 0) ? 20 : 1;
            // slack = -slack;
            // slack = -slack+min_slack;
            // slack = -slack+min_slack+1;
            // slack = std::exp(-slack);
            // slack = std::exp(-0.001*slack);
        }
        double x_sum = 0;
        double y_sum = 0;
        double total_slack = 0;
        for (size_t i = 0; i < x_list.size(); i++)
        {
            x_sum += x_list[i] * slack_list[i];
            y_sum += y_list[i] * slack_list[i];
            total_slack += slack_list[i];
        }
        if (total_slack == 0)
        {
            std::cout << "Error: total slack is 0" << std::endl;
            exit(1);
        }
        int x_avg = x_sum / total_slack;
        int y_avg = y_sum / total_slack;
        Site* nearest_site = _siteMap->getNearestSite(x_avg, y_avg);
        result_x = nearest_site->getX();
        result_y = nearest_site->getY();
    }
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
    
    removeCell(ff1);
    removeCell(ff2);

    int leftDownX = std::min(ff1->getX(), ff2->getX());
    int leftDownY = std::min(ff1->getY(), ff2->getY());
    int rightUpX = std::max(ff1->getX() + ff1->getWidth(), ff2->getX() + ff2->getWidth());
    int rightUpY = std::max(ff1->getY() + ff1->getHeight(), ff2->getY() + ff2->getHeight());
    double min_gain = -INFINITY;

    // set candidates
    std::vector<std::pair<int, int>> candidates;
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
        if(gain > min_gain)
        {
            min_gain = gain;
            result_x = target_x;
            result_y = target_y;
        }
    }

    placeCell(ff1);
    placeCell(ff2);
    
    return min_gain + remove_gain;
}

void Solver::greedyBanking(std::vector<std::vector<FF*>> clusters)
{
    // #pragma omp parallel for num_threads(NUM_THREADS)
    for(auto cluster : clusters)
    {
        if(cluster.size() < 2)
            continue;
        // find the best pair to bank
        // TODO: Complete the algorithm
        double maxGain;
        do
        {
            maxGain = 0.0;
            int result_x = 0;
            int result_y = 0;
            FF* bestFF1 = nullptr;
            FF* bestFF2 = nullptr;
            LibCell* targetFF = nullptr;
            for(size_t i = 0; i < cluster.size(); i++)
            {
                for(size_t j = i+1; j < cluster.size(); j++)
                {
                    FF* ff1 = cluster[i];
                    FF* ff2 = cluster[j];
                    for(auto ff : _ffsLibList)
                    {
                        if(ff->bit == ff1->getBit() + ff2->getBit())
                        {
                            int x, y;
                            double gain = cal_banking_gain(ff1, ff2, ff, x, y);
                            if(gain > maxGain || bestFF1 == nullptr)
                            {
                                maxGain = gain;
                                bestFF1 = ff1;
                                bestFF2 = ff2;
                                targetFF = ff;
                                result_x = x;
                                result_y = y;
                            }
                        }
                    }
                }
            }
            // #pragma omp critical
            if (maxGain > 0)
            {
                bankFFs(bestFF1, bestFF2, targetFF, result_x, result_y);
                cluster.erase(std::remove(cluster.begin(), cluster.end(), bestFF1), cluster.end());
                cluster.erase(std::remove(cluster.begin(), cluster.end(), bestFF2), cluster.end());
            }
        } while (maxGain > 0);
    }
}

void Solver::display()
{
    using namespace std;
    // Display cost metrics
    cout << "Cost metrics:" << endl;
    cout << "ALPHA: " << ALPHA << endl;
    cout << "BETA: " << BETA << endl;
    cout << "GAMMA: " << GAMMA << endl;
    cout << "LAMBDA: " << LAMBDA << endl;
    cout << "====================================" << endl;
    // Display die info
    cout << "Die info:" << endl;
    cout << "DIE_LOW_LEFT_X: " << DIE_LOW_LEFT_X << endl;
    cout << "DIE_LOW_LEFT_Y: " << DIE_LOW_LEFT_Y << endl;
    cout << "DIE_UP_RIGHT_X: " << DIE_UP_RIGHT_X << endl;
    cout << "DIE_UP_RIGHT_Y: " << DIE_UP_RIGHT_Y << endl;
    cout << "====================================" << endl;
    // Display I/O info
    cout << "Input pins:" << endl;
    for(auto pin : _inputPins)
    {
        cout << "Name: " << pin->getName() << ", X: " << pin->getX() << ", Y: " << pin->getY() << endl;
    }
    cout << "Output pins:" << endl;
    for(auto pin : _outputPins)
    {
        cout << "Name: " << pin->getName() << ", X: " << pin->getX() << ", Y: " << pin->getY() << endl;
    }
    cout << "====================================" << endl;
    // Display cell library
    cout << "FlipFlops libs:" << endl;
    for(auto ff : _ffsLibList)
    {
        cout << "Name: " << ff->cell_name << ", Width: " << ff->width << ", Height: " << ff->height << ", Bits: " << ff->bit << endl;
        cout << "Qpin delay: " << ff->qDelay << endl;
        cout << "Power: " << ff->power << endl;
        for(auto pin : ff->pins)
        {
            cout << "Pin Name: " << pin->getName() << ", X: " << pin->getX() << ", Y: " << pin->getY() << endl;
        }
    }
    cout << "Gates libs:" << endl;
    for(auto comb : _combsLibList)
    {
        cout << "Name: " << comb->cell_name << ", Width: " << comb->width << ", Height: " << comb->height << endl;
        cout << "Power: " << comb->power << endl;
        for(auto pin : comb->pins)
        {
            cout << "Pin Name: " << pin->getName() << ", X: " << pin->getX() << ", Y: " << pin->getY() << endl;
        }
    }
    cout << "====================================" << endl;
    // Display instance info
    cout << "Instances:" << endl;
    cout << "FlipFlops:" << endl;
    for(auto ff : _ffs)
    {
        cout << "Name: " << ff->getInstName() << ", Lib: " << ff->getCellName() << ", X: " << ff->getX() << ", Y: " << ff->getY() << ", Width: " << ff->getWidth() << ", Height: " << ff->getHeight() << ", Bits: " << ff->getBit() << endl;
        cout << "Qpin delay: " << ff->getQDelay() << endl;
        cout << "Power: " << ff->getPower() << endl;
        for(auto pin : ff->getPins())
        {
            cout << "Pin Name: " << pin->getName() << ", X: " << pin->getX() << ", Y: " << pin->getY();
            if(pin->isDpin())
                cout << ", Slack: " << pin->getSlack();
            cout << endl;
        }
    }
    cout << "Gates:" << endl;
    for(auto comb : _combs)
    {
        cout << "Name: " << comb->getInstName() << ", Lib: " << comb->getCellName() << ", X: " << comb->getX() << ", Y: " << comb->getY() << ", Width: " << comb->getWidth() << ", Height: " << comb->getHeight() << endl;
        for(auto pin : comb->getPins())
        {
            cout << "Pin Name: " << pin->getName() << ", X: " << pin->getX() << ", Y: " << pin->getY() << endl;
        }
    }
    cout << "====================================" << endl;
    // Display net info
    cout << "Nets:" << endl;
    for(auto net : _nets)
    {
        cout << "Name: " << net->getName() << endl;
        for(auto pin : net->getPins())
        {
            cout << "Pin Name: " << pin->getName() << ", X: " << pin->getX() << ", Y: " << pin->getY() << endl;
        }
    }
    cout << "====================================" << endl;
    // Display bin info
    cout << "Bin info:" << endl;
    cout << "BIN_WIDTH: " << BIN_WIDTH << endl;
    cout << "BIN_HEIGHT: " << BIN_HEIGHT << endl;
    cout << "BIN_MAX_UTIL: " << BIN_MAX_UTIL << endl;
    cout << "====================================" << endl;
    // Display placement rows
    cout << "Placement rows:" << endl;
    for(auto row : _placementRows)
    {
        cout << "StartX: " << row.startX << ", StartY: " << row.startY << ", SiteWidth: " << row.siteWidth << ", SiteHeight: " << row.siteHeight << ", NumSites: " << row.numSites << endl;
    }
    cout << "====================================" << endl;
    // Display timing info
    cout << "Timing info:" << endl;
    cout << "DISP_DELAY: " << DISP_DELAY << endl;
    cout << "====================================" << endl;
    // Display bin utilization
    cout << "Bin utilization:" << endl;
    for(auto row : _binMap->getBins())
    {
        cout << "Bin: (" << row->getX() << ", " << row->getY() << "), Utilization: " << row->getUtilization() << endl;
    }
    // Display placement row
    cout << "Placement rows:" << endl;
    std::vector<Site*> sites = _siteMap->getSites();
    for(auto site : sites)
    {
        cout << "Site: (" << site->getX() << ", " << site->getY() << "), Width: " << site->getWidth() << ", Height: " << site->getHeight()<<", NumCells: "<< site->getNumCells() << endl;
    }

}

void Solver::dump(std::string filename) const
{
    using namespace std;
    ofstream out(filename);
    out << "CellInst " << _ffs.size() << endl;
    for (auto ff : _ffs)
    {
        out << "Inst " << ff->getInstName() << " " << ff->getCellName() << " " << ff->getX() << " " << ff->getY() << endl;
    }
    for (auto ff : _ffs)
    {
        for (auto pin : ff->getInputPins())
        {
            out << pin->getOriginalName() << " map " << pin->getCell()->getInstName() << "/" << pin->getName() << endl;
        }
        for (auto pin : ff->getOutputPins())
        {
            out << pin->getOriginalName() << " map " << pin->getCell()->getInstName() << "/" << pin->getName() << endl;
        }
        Pin* clkPin = ff->getClkPin();
        std::vector<std::string> clkPinNames = ff->getClkPin()->getOriginalNames();
        for(auto clkPinName : clkPinNames)
        {
            out << clkPinName << " map " << clkPin->getCell()->getInstName() << "/" << clkPin->getName() << endl;
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