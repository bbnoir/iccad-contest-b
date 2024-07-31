#include "Solver.h"
#include "Cell.h"
#include "Comb.h"
#include "FF.h"
#include "Net.h"
#include "Pin.h"
#include "Site.h"
#include "Bin.h"
#include "Legalizer.h"

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
        cout << "Error opening file: " << filename << endl;
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
                if (pinName[0] == 'D') {
                    ff->inputPins.push_back(new Pin(PinType::FF_D, x, y, pinName, nullptr));
                } else if (pinName[0] == 'Q') {
                    ff->outputPins.push_back(new Pin(PinType::FF_Q, x, y, pinName, nullptr));
                } else if (pinName[0] == 'C') {
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
                if (pinName[0] == 'I') {
                    comb->inputPins.push_back(new Pin(PinType::GATE_IN, x, y, pinName, nullptr));
                } else if (pinName[0] == 'O') {
                    comb->outputPins.push_back(new Pin(PinType::GATE_OUT, x, y, pinName, nullptr));
                }
            }
            // sort the pins by name
            sort(comb->inputPins.begin(), comb->inputPins.end(), [](Pin* a, Pin* b) -> bool { return a->getName() < b->getName(); });
            sort(comb->outputPins.begin(), comb->outputPins.end(), [](Pin* a, Pin* b) -> bool { return a->getName() < b->getName(); });
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
            if(pin == "CLK" && !clknet){
                clknet = true;
                _ffs_clkdomains.push_back(vector<FF*>());
            }
            Pin* p = nullptr;
            if(_ffsMap.find(instName) != _ffsMap.end()){
                p = _ffsMap[instName]->getPin(pin);
                if(pin == "CLK" && clknet){
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
                cout << "Pin not found: " << pinName << endl;
                exit(1);
            }
            pins.push_back(p);
            p->connect(newNet);
        }
        newNet->setPins(pins);
        _nets.push_back(newNet);
        _netsMap[netName] = newNet;
        // update pin fanin and fanout
        for(auto pin : pins)
        {
            // assume the first pin is the fanin pin
            if(pin->getType() == PinType::FF_D || pin->getType() == PinType::GATE_IN || pin->getType() == PinType::FF_CLK)
            {
                pin->setFaninPin(pins[0]);
            }else if(pin->getType() == PinType::FF_Q || pin->getType() == PinType::GATE_OUT || pin->getType() == PinType::INPUT)
            {
                for(long unsigned int j = 1; j < pins.size(); j++)
                {
                    pin->addFanoutPin(pins[j]);
                }
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
    for(long unsigned int i = 0; i < _ffsLibList.size(); i++)
    {
        string cellName;
        double delay;
        in >> token >> cellName >> delay;
        _ffsLibMap[cellName]->qDelay = delay;
    }
    // slack
    for(long unsigned int i = 0; i < _ffs.size(); i++)
    {
        string instName;
        string port;
        double slack;
        in >> token >> instName >> port >> slack;
        _ffsMap[instName]->getPin(port)->setInitSlack(slack);
        if (_ffsMap[instName]->getBit() > 1)
        {
            for (int j = 1; j < _ffsMap[instName]->getBit(); j++)
            {
                in >> token >> instName >> port >> slack;
                _ffsMap[instName]->getPin(port)->setInitSlack(slack);
            }
        }
    }
    // Read power info
    for(long unsigned int i = 0; i < _ffsLibList.size(); i++)
    {
        string cellName;
        double power;
        in >> token >> cellName >> power;
        if(_ffsLibMap.find(cellName) != _ffsLibMap.end())
        {
            _ffsLibMap[cellName]->power = power;
        }else if(_combsLibMap.find(cellName) != _combsLibMap.end())
        {
            _combsLibMap[cellName]->power = power;
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
                    if (curType == PinType::FF_Q)
                    {
                        inPin->addPrevStagePin(curPin, pinStack);
                        curPin->addNextStagePin(inPin);
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
                    else if (curType == PinType::INPUT)
                    {
                        inPin->addPrevStagePin(curPin, pinStack);
                        curPin->addNextStagePin(inPin);
                    }
                    else if (curType == PinType::FF_D)
                    {
                        break;
                    }
                    else
                    {
                        std::cout << "Error: Unexpected fanin pin type" << endl;
                        std::cout << "Pin: " << curPin->getCell()->getInstName() << " " << curPin->getName() << endl;
                        exit(1);
                    }
                }
            }
            inPin->initArrivalTime();
            inPin->initCriticalIndex();
        }
    }

    cout << "File parsed successfully" << endl;
    in.close();
}

double Solver::calCost()
{
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
    std::cout<<"Alpha: "<<ALPHA<<" ";
    std::cout<<"Beta: "<<BETA<<" ";
    std::cout<<"Gamma: "<<GAMMA<<" ";
    std::cout<<"Lambda: "<<LAMBDA<<std::endl;
    std::cout<<"TNS: "<<tns<<std::endl;
    std::cout<<"Power: "<<power<<std::endl;
    std::cout<<"Area: "<<area<<std::endl;
    std::cout<<"Num of bins violated: "<<numOfBinsViolated<<std::endl;
    double cost = ALPHA * tns + BETA * power + GAMMA * area + LAMBDA * numOfBinsViolated;
    // std::cout<<"Cost: "<<cost<<std::endl;
    return cost;
}

void Solver::checkCLKDomain()
{
    constructFFsCLKDomain();
    int sum = 0;
    for(long unsigned int i=0;i<_ffs_clkdomains.size();i++){
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
        std::cerr << "Cell not placed on site: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell in the die
    if(cell->getX() < DIE_LOW_LEFT_X || cell->getX()+cell->getWidth() > DIE_UP_RIGHT_X || cell->getY() < DIE_LOW_LEFT_Y || cell->getY()+cell->getHeight() > DIE_UP_RIGHT_Y)
    {
        std::cerr << "Cell not in die: " << cell->getInstName() << std::endl;
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
        std::cerr << "Cell not placed on site: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell in the die
    if(x < DIE_LOW_LEFT_X || x+cell->getWidth() > DIE_UP_RIGHT_X || y < DIE_LOW_LEFT_Y || y+cell->getHeight() > DIE_UP_RIGHT_Y)
    {
        std::cerr << "Cell not in die: " << cell->getInstName() << std::endl;
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
        if(isOverlap(x, y, cell, c))
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
        std::cerr << "Cell not placed on site: " << cell->getInstName() << std::endl;
        return false;
    }
    // check the cell in the die
    if(x < DIE_LOW_LEFT_X || x+cell->getWidth() > DIE_UP_RIGHT_X || y < DIE_LOW_LEFT_Y || y+cell->getHeight() > DIE_UP_RIGHT_Y)
    {
        std::cerr << "Cell not in die: " << cell->getInstName() << std::endl;
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
    removeCell(cell);
    cell->setX(x);
    cell->setY(y);
    placeCell(cell);
}

double Solver::calCostMoveD(Pin* movedDPin, int sourceX, int sourceY, int targetX, int targetY)
{
    if (sourceX == targetX && sourceY == targetY)
    {
        return 0;
    }
    const int faninpin_x = movedDPin->getFaninPin()->getGlobalX();
    const int faninpin_y = movedDPin->getFaninPin()->getGlobalY();
    const int diff_dist = abs(targetX - faninpin_x) + abs(targetY - faninpin_y) - abs(sourceX - faninpin_x) - abs(sourceY - faninpin_y);
    const double old_slack = movedDPin->getSlack();
    const double new_slack = old_slack - DISP_DELAY * diff_dist;
    return calDiffCost(old_slack, new_slack);
}

double Solver::calCostMoveQ(Pin* movedQPin, int sourceX, int sourceY, int targetX, int targetY)
{
    if (sourceX == targetX && sourceY == targetY)
    {
        return 0;
    }
    double diff_cost = 0;
    for (auto nextStagePin : movedQPin->getNextStagePins())
    {
        if (nextStagePin->getType() == PinType::FF_D)
        {
            const double next_old_slack = nextStagePin->getSlack();
            const double next_new_slack = nextStagePin->calSlack(movedQPin, sourceX, sourceY, targetX, targetY);
            diff_cost += calDiffCost(next_old_slack, next_new_slack);
        }
    }
    return diff_cost;
}

double Solver::updateCostMoveD(Pin* movedDPin, int sourceX, int sourceY, int targetX, int targetY)
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
    movedDPin->setSlack(new_slack);
    double diff_cost = calDiffCost(old_slack, new_slack);
    _currCost += diff_cost;
    return diff_cost;
}

double Solver::updateCostMoveQ(Pin* movedQPin, int sourceX, int sourceY, int targetX, int targetY)
{
    if (sourceX == targetX && sourceY == targetY)
    {
        return 0;
    }
    double diff_cost = 0;
    for (auto nextStagePin : movedQPin->getNextStagePins())
    {
        if (nextStagePin->getType() == PinType::FF_D && (nextStagePin->getCell() != movedQPin->getCell() || nextStagePin->getFaninPin() != movedQPin))
        {
            const double next_old_slack = nextStagePin->getSlack();
            const double next_new_slack = nextStagePin->updateSlack(movedQPin, sourceX, sourceY, targetX, targetY);
            diff_cost += calDiffCost(next_old_slack, next_new_slack);
        }
    }
    _currCost += diff_cost;
    return diff_cost;
}

double Solver::calCostChangeQDelay(Pin* changedQPin, double diffQDelay)
{
    double diff_cost = 0;
    for (auto nextStagePin : changedQPin->getNextStagePins())
    {
        if (nextStagePin->getType() == PinType::FF_D)
        {
            const double next_old_slack = nextStagePin->getSlack();
            const double next_new_slack = nextStagePin->calSlackQ(changedQPin, diffQDelay);
            diff_cost += calDiffCost(next_old_slack, next_new_slack);
        }
    }
    return diff_cost;
}

double Solver::updateCostChangeQDelay(Pin* changedQPin, double diffQDelay)
{
    double diff_cost = 0;
    for (auto nextStagePin : changedQPin->getNextStagePins())
    {
        if (nextStagePin->getType() == PinType::FF_D)
        {
            const double next_old_slack = nextStagePin->getSlack();
            const double next_new_slack = nextStagePin->updateSlackQ(changedQPin, diffQDelay);
            diff_cost += calDiffCost(next_old_slack, next_new_slack);
        }
    }
    _currCost += diff_cost;
    return diff_cost;
}

/*
Update current cost after moving the ff
*/
double Solver::calCostMoveFF(FF* movedFF, int sourceX, int sourceY, int targetX, int targetY)
{
    if (sourceX == targetX && sourceY == targetY)
    {
        return 0;
    }
    // std::cout << "Update cost for " << movedFF->getInstName() << " from (" << sourceX << ", " << sourceY << ") to (" << targetX << ", " << targetY << ")" << std::endl;
    double diff_cost = 0;
    for (auto inPin : movedFF->getInputPins())
    {
        diff_cost += calCostMoveD(inPin, sourceX + inPin->getX(), sourceY + inPin->getY(), targetX + inPin->getX(), targetY + inPin->getY());
    }
    for (auto outPin : movedFF->getOutputPins())
    {
        diff_cost += calCostMoveQ(outPin, sourceX + outPin->getX(), sourceY + outPin->getY(), targetX + outPin->getX(), targetY + outPin->getY());
    }
    // std::cout << "Diff cost: " << diff_cost << std::endl << std::endl;
    return diff_cost;
}

/*
Update current cost after moving the ff
*/
double Solver::updateCostMoveFF(FF* movedFF, int sourceX, int sourceY, int targetX, int targetY)
{
    if (sourceX == targetX && sourceY == targetY)
    {
        return 0;
    }
    // std::cout << "Update cost for " << movedFF->getInstName() << " from (" << sourceX << ", " << sourceY << ") to (" << targetX << ", " << targetY << ")" << std::endl;
    double diff_cost = 0;
    for (auto inPin : movedFF->getInputPins())
    {
        diff_cost += updateCostMoveD(inPin, sourceX + inPin->getX(), sourceY + inPin->getY(), targetX + inPin->getX(), targetY + inPin->getY());
    }
    for (auto outPin : movedFF->getOutputPins())
    {
        diff_cost += updateCostMoveQ(outPin, sourceX + outPin->getX(), sourceY + outPin->getY(), targetX + outPin->getX(), targetY + outPin->getY());
    }
    // std::cout << "Diff cost: " << diff_cost << std::endl << std::endl;
    return diff_cost;
}

double Solver::updateCostBankFF(FF* ff1, FF* ff2, LibCell* targetFF, int targetX, int targetY)
{
    double diff_cost = 0;
    diff_cost += (targetFF->power - ff1->getPower() - ff2->getPower()) * BETA;
    diff_cost += (targetFF->width * targetFF->height - ff1->getArea() - ff2->getArea()) * GAMMA;
    _currCost += diff_cost;
    const int ff1_bit = ff1->getBit();
    const int ff2_bit = ff2->getBit();
    // D pin
    for (int i = 0; i < ff1_bit+ff2_bit; i++)
    {
        int op_idx = (i < ff1_bit) ? i : i - ff1_bit;
        FF* workingFF = (i < targetFF->bit) ? ff1 : ff2;
        Pin* inPin = workingFF->getInputPins()[op_idx];
        Pin* mapInPin = targetFF->inputPins[op_idx];
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
            inPin->setSlack(new_slack);
            const double d_cost = calDiffCost(old_slack, new_slack);
            _currCost += d_cost;
            diff_cost += d_cost;
        }
        else
        {
            diff_cost += updateCostMoveD(inPin, inPin->getGlobalX(), inPin->getGlobalY(), targetX + mapInPin->getX(), targetY + mapInPin->getY());
        }
    }
    // Q pin
    for (int i = 0; i < ff1_bit+ff2_bit; i++)
    {
        int op_idx = (i < ff1_bit) ? i : i - ff1_bit;
        FF* workingFF = (i < targetFF->bit) ? ff1 : ff2;
        Pin* outPin = workingFF->getOutputPins()[op_idx];
        Pin* mapOutPin = targetFF->outputPins[op_idx];
        diff_cost += updateCostChangeQDelay(outPin, targetFF->qDelay - workingFF->getQDelay());
        for (auto nextStagePin : outPin->getNextStagePins())
        {
            if (nextStagePin->getType() == PinType::FF_D)
            {
                if (nextStagePin->getCell() == ff1 || nextStagePin->getCell() == ff2)
                {
                    if (nextStagePin->getFaninPin()->getCell() == ff1 || nextStagePin->getFaninPin()->getCell() == ff2)
                    {
                        // already considered in the D pin
                        break;
                    }
                    const double next_old_slack = nextStagePin->getSlack();
                    std::vector<double> arrivalTimes = nextStagePin->getArrivalTimesRef();
                    std::vector<int> sortedCriticalIndex = nextStagePin->getSortedCriticalIndexRef();
                    const double old_arrival_time = arrivalTimes.at(sortedCriticalIndex.at(0));
                    // update the arrival time and re-sort the critical index
                    std::vector<int> indexList = nextStagePin->getPathIndex(outPin);
                    for (int index : indexList)
                    {
                        std::vector<Pin*> path = nextStagePin->getPathToPrevStagePins(index);
                        Pin* secondLastPin = path.at(path.size()-2);
                        const int secondLastPinX = secondLastPin->getGlobalX();
                        const int secondLastPinY = secondLastPin->getGlobalY();
                        const double old_arrival_time = arrivalTimes.at(index);
                        const double diff_arrival_time = DISP_DELAY * (abs(outPin->getGlobalX() - secondLastPinX) + abs(outPin->getGlobalY() - secondLastPinY) - abs(targetX + mapOutPin->getX() - secondLastPinX) - abs(targetY + mapOutPin->getY() - secondLastPinY));
                        const double new_arrival_time = old_arrival_time - diff_arrival_time;
                        arrivalTimes.at(index) = new_arrival_time;
                        // reheap the new arrival time to the sorted list
                        int sortIndex = 0;
                        while (sortedCriticalIndex.at(sortIndex) != index)
                        {
                            sortIndex++;
                        }
                        std::push_heap(sortedCriticalIndex.begin(), sortedCriticalIndex.begin()+sortIndex+1, [&arrivalTimes](int i, int j) {
                            return arrivalTimes.at(i) < arrivalTimes.at(j);
                        });
                    }
                    const double new_arrival_time = arrivalTimes.at(sortedCriticalIndex.at(0));
                    const double next_new_slack =  next_old_slack + (old_arrival_time - new_arrival_time);
                    const double d_cost = calDiffCost(next_old_slack, next_new_slack);
                    _currCost += d_cost;
                    diff_cost += d_cost;
                }
                else
                {
                    const double next_old_slack = nextStagePin->getSlack();
                    const double next_new_slack = nextStagePin->updateSlack(outPin, outPin->getGlobalX(), outPin->getGlobalY(), targetX + mapOutPin->getX(), targetY + mapOutPin->getY());
                    const double d_cost = calDiffCost(next_old_slack, next_new_slack);
                    _currCost += d_cost;
                    diff_cost += d_cost;
                }
            }
        }
    }
    return diff_cost;
}

void Solver::resetSlack()
{
    for (auto ff : _ffs)
    {
        for (auto inPin : ff->getInputPins())
        {
            inPin->resetSlack();
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

void Solver::bankFFs(FF* ff1, FF* ff2, LibCell* targetFF)
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
    findForceDirectedPlacementBankingFFs(ff1, ff2, target_x, target_y);
    updateCostBankFF(ff1, ff2, targetFF, target_x, target_y);
    FF* bankedFF = new FF(target_x, target_x, makeUniqueName(), targetFF, dqPairs, clkPins);
    bankedFF->setClkDomain(ff1->getClkDomain());
    addFF(bankedFF);
    placeCell(bankedFF);
    // free up old ffs
    deleteFF(ff1);
    deleteFF(ff2);
}

std::string Solver::makeUniqueName()
{
    std::string newName = "A" + std::to_string(uniqueNameCounter++);
    return newName;
}

void Solver::chooseBaseFF()
{
    // TODO: change the way to choose the base FF
    std::vector<LibCell*> oneBitFFs;
    for (auto ff : _ffsLibList)
    {
        if(ff->bit == 1)
        {
            oneBitFFs.push_back(ff);
        }
    }
    // choose the FF with the smallest cost
    double minCost = ALPHA * oneBitFFs[0]->qDelay + BETA * oneBitFFs[0]->power + GAMMA * oneBitFFs[0]->width * oneBitFFs[0]->height;
    _baseFF = oneBitFFs[0];
    for (auto ff : oneBitFFs)
    {
        double cost = ALPHA * ff->qDelay + BETA * ff->power + GAMMA * ff->width * ff->height;
        if(cost < minCost)
        {
            minCost = cost;
            _baseFF = ff;
        }
    }
    std::cout << "Base FF: " << _baseFF->cell_name << std::endl;
}

void Solver::debankAll()
{
    std::vector<FF*> debankedFFs;
    for (auto ff : _ffs)
    {
        removeCell(ff);
    }
    const double baseArea = _baseFF->width * _baseFF->height;
    const double basePower = _baseFF->power;
    for (auto ff : _ffs)
    {
        // update cost on area and power
        const int n_bit = ff->getBit();
        _currCost += BETA * (basePower * n_bit - ff->getPower()) + GAMMA * (baseArea * n_bit - ff->getArea());
        // debank the FF and update slack
        std::vector<std::pair<Pin*, Pin*>> dqPairs = ff->getDQpairs();
        Pin* clkPin = ff->getClkPin();
        const int x = ff->getX();
        const int y = ff->getY();
        const int d_x = x + _baseFF->inputPins[0]->getX();
        const int d_y = y + _baseFF->inputPins[0]->getY();
        const int q_x = x + _baseFF->outputPins[0]->getX();
        const int q_y = y + _baseFF->outputPins[0]->getY();
        const double diffQDelay = _baseFF->qDelay - ff->getQDelay();
        const int clkDomain = ff->getClkDomain();
        // TODO: update the slack
        for (auto dq : dqPairs)
        {
            Pin *d = dq.first, *q = dq.second;
            updateCostMoveD(d, x + d->getX(), y + d->getY(), d_x, d_y);
            updateCostMoveQ(q, x + q->getX(), y + q->getY(), q_x, q_y);
            updateCostChangeQDelay(q, diffQDelay);
            FF* newFF = new FF(x, y, makeUniqueName(), _baseFF, dq, clkPin);
            newFF->setClkDomain(clkDomain);
            debankedFFs.push_back(newFF);
        }
        delete clkPin;
    }
    for(long unsigned int i=0;i<_ffs.size();i++)
    {
        delete _ffs[i];
    }
    _ffs.clear();
    _ffsMap.clear();
    for (auto ff : debankedFFs)
    {
        addFF(ff);
        placeCell(ff);
    }
}

void Solver::forceDirectedPlaceFF(FF* ff)
{
    // calculate the force
    double x_sum = 0.0;
    double y_sum = 0.0;
    int num_pins = 0;
    for (auto inPin : ff->getInputPins())
    {
        Pin* fanin = inPin->getFaninPin();
        x_sum += fanin->getGlobalX();
        y_sum += fanin->getGlobalY();
        num_pins++;
    }
    for (auto outPin : ff->getOutputPins())
    {
        for (auto fanout : outPin->getFanoutPins())
        {
            x_sum += fanout->getGlobalX();
            y_sum += fanout->getGlobalY();
            num_pins++;            
        }
    }
    int x_avg = x_sum / num_pins;
    int y_avg = y_sum / num_pins;
    // adjust it to fit the placement rows
    Site* nearest_site = _siteMap->getNearestSite(x_avg, y_avg);
    // move the ff
    moveCell(ff, nearest_site->getX(), nearest_site->getY());
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
    double x_sum = 0.0;
    double y_sum = 0.0;
    int num_pins = 1;
    FF* ff = _ffs[ff_idx];
    // display ff's info
    bool isAllConnectedToComb = true;
    for (auto inPin : ff->getInputPins())
    {
        Pin* fanin = inPin->getFaninPin();
        x_sum += fanin->getGlobalX();
        y_sum += fanin->getGlobalY();
        num_pins++;
        if (fanin->getType() == PinType::FF_Q)
        {
            isAllConnectedToComb = false;
        }
    }
    for (auto outPin : ff->getOutputPins())
    {
        for (auto fanout : outPin->getFanoutPins())
        {
            x_sum += fanout->getGlobalX();
            y_sum += fanout->getGlobalY();
            num_pins++;            
            if (fanout->getType() == PinType::FF_D)
            {
                isAllConnectedToComb = false;
            }
        }
    }
    int x_avg = x_sum / num_pins;
    int y_avg = y_sum / num_pins;
    Site* nearest_site = _siteMap->getNearestSite(x_avg, y_avg);
    const int source_x = ff->getX();
    const int source_y = ff->getY();
    const int target_x = nearest_site->getX();
    const int target_y = nearest_site->getY();
    // TODO: maybe change to manhattan distance
    double dist = sqrt(pow(source_x - target_x, 2) + pow(source_y - target_y, 2));
    moveCell(ff, target_x, target_y);
    updateCostMoveFF(ff, source_x, source_y, target_x, target_y);

    // check if the ff should be locked
    if (dist < 1e-2)
    {
        lock_cnt[ff_idx]++;
    }
    if (isAllConnectedToComb || lock_cnt[ff_idx] > 3)
    {
        locked[ff_idx] = true;
    }
    if (locked[ff_idx])
    {
        lock_num++;
    }
}

void Solver::forceDirectedPlacement()
{
    // std::cout << "Force directed placement" << std::endl;
    bool converged = false;
    int max_iter = 1000;
    int iter = 0;
    const int numFFs = _ffs.size();
    const int lockThreshold = numFFs;
    // std::cout << "Lock threshold: " << lockThreshold << std::endl;
    int lock_num = 0;
    std::vector<char> lock_cnt(numFFs, 0);
    std::vector<bool> locked(numFFs, false);
    while(!converged && iter++ < max_iter){
        for (int i = 0; i < numFFs; i++)
        {
            forceDirectedPlaceFFLock(i, locked, lock_cnt, lock_num);
        }
        // std::cout << "Iteration: " << iter << ", Lock/Total: " << lock_num << "/" << numFFs << std::endl;
        converged = lock_num >= lockThreshold;
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
            for(long unsigned int i = 0; i < bins.size(); i++)
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
            
            for(long unsigned int i = 0; i < ffs_local.size(); i++)
            {
                FF* ff = ffs_local[i];
                double cost_min = 0;
                int best_site = -1;
                for(long unsigned int j = 0; j < sites.size(); j++)
                {
                    if(!placeable(ff, sites[j]->getX(), sites[j]->getY()))
                        continue;
                    
                    // TODO: cost is depend on the slack and bin utilization rate
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

                    double slackCost = calCostMoveFF(ff, original_x, original_y, trial_x, trial_y);

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
                    moveCell(ff, sites[best_site]->getX(), sites[best_site]->getY());
                    updateCostMoveFF(ff, ff->getX(), ff->getY(), sites[best_site]->getX(), sites[best_site]->getY());
                }
            }
        }
    }
    std::cout << "Cost difference: " << cost_difference << std::endl;
}

void Solver::solve()
{
    init_placement();
    
    _initCost = calCost();
    _currCost = _initCost;
    std::cout << "==> Initial cost: " << _initCost << std::endl;

    // TODO: placing FFs on the die when global placement is unnecessary 
    chooseBaseFF();
    debankAll();
    std::cout << "==> Cost after debanking: " << _currCost << std::endl;
    resetSlack();
    _currCost = calCost();
    std::cout << "==> Cost after reset slack: " << _currCost << std::endl;

    std::cout << "There are "<<_binMap->getNumOverMaxUtilBins()<<" over utilized bins initially."<<std::endl; 
    std::cout << _binMap->getNumOverMaxUtilBinsByComb() << " of them are over utilized by Combs." << std::endl;
    
    std::cout<<"Start to force directed placement"<<std::endl;
    forceDirectedPlacement();
    std::cout << "==> Cost after force directed placement: " << _currCost << std::endl;
    resetSlack();
    _currCost = calCost();
    std::cout << "==> Cost after reset slack: " << _currCost << std::endl;
    
    std::cout << "Start clustering and banking" << std::endl;
    size_t prev_ffs_size;
    std::cout << "FFs size: " << _ffs.size() << std::endl;
    do
    {
        constructFFsCLKDomain();
        prev_ffs_size = _ffs.size();
        for(long unsigned int i = 0; i < _ffs_clkdomains.size(); i++)
        {
            std::vector<std::vector<FF*>> cluster = clusteringFFs(i);
            greedyBanking(cluster);
        }
        std::cout << "FFs size after greedy banking: " << _ffs.size() << std::endl;
    } while (prev_ffs_size != _ffs.size());

    std::cout << "==> Cost after clustering and banking: " << _currCost << std::endl;
    resetSlack();
    _currCost = calCost();
    std::cout << "==> Cost after reset slack: " << _currCost << std::endl;
    
    std::cout << "Start to force directed placement (second)" << std::endl;
    forceDirectedPlacement();
    std::cout << "==> Cost after force directed placement (second): " << _currCost << std::endl;
    resetSlack();
    _currCost = calCost();
    std::cout << "==> Cost after reset slack: " << _currCost << std::endl;
    
    std::cout<<"Start to legalize"<<std::endl;
    _legalizer->legalize();
    resetSlack();
    _currCost = calCost();
    std::cout << "==> Cost after reset slack: " << _currCost << std::endl;

    std::cout << "There are "<<_binMap->getNumOverMaxUtilBins()<<" over utilized bins after Legalization."<<std::endl; 

    std::cout<<"Start to fine tune"<<std::endl;
    fineTune();
    resetSlack();
    _currCost = calCost();
    std::cout << "==> Cost after reset slack: " << _currCost << std::endl;

    std::cout << "There are "<<_binMap->getNumOverMaxUtilBins()<<" over utilized bins after Fine Tuning."<<std::endl;
}

/*
Check for FFs in Die, FFs on Site, and Overlap
*/
void Solver::check()
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
    }
}

/*
Evaluate all kinds of cost
*/
void Solver::evaluate()
{
    std::cout << "============ Start evaluating ============" << std::endl;
    // double totalCost = 0.0;
    // double totalArea = 0.0;
    // double totalPower = 0.0;
    // double totalNegSlack = 0.0;
    // int numOfOverUtilBins = _binMap->getNumOverMaxUtilBins();
    // std::cout << "Number of over utilized bins: " << numOfOverUtilBins << std::endl;
    std::cout << "============= End evaluating =============" << std::endl;
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
    for(long unsigned int i = 0; i < cells.size(); i++)
    {
        // for(long unsigned int j = i+1; j < cells.size(); j++)
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

std::vector<int> Solver::regionQuery(std::vector<FF*> FFs, long unsigned int idx, int eps)
{
    std::vector<int> neighbors;
    for(long unsigned int i = 0; i < FFs.size(); i++)
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

std::vector<std::vector<FF*>> Solver::clusteringFFs(long unsigned int clkdomain_idx)
{
    if(clkdomain_idx >= _ffs_clkdomains.size())
    {
        std::cerr << "Invalid clk domain index" << std::endl;
        return std::vector<std::vector<FF*>>();
    }
    std::vector<FF*> FFs = _ffs_clkdomains[clkdomain_idx];
    std::vector<std::vector<FF*>> clusters;
    std::vector<bool> visited(FFs.size(), false);
    // DBSCAN
    // TODO: optimize the algorithm or change to other clustering algorithm
    for(long unsigned int i = 0; i < FFs.size(); i++)
    {
        if(visited[i])
            continue;
        visited[i] = true;
        std::vector<FF*> cluster;
        cluster.push_back(FFs[i]);
        std::vector<int> neighbors = regionQuery(FFs, i, 10000);
        if(neighbors.size() < 2)
        {
            // noise
            clusters.push_back(cluster);
            continue;
        }
        for(long unsigned int j = 0; j < neighbors.size(); j++)
        {
            int idx = neighbors[j];
            if (visited[idx])
                continue;
            if(!visited[idx])
            {
                visited[idx] = true;
                std::vector<int> new_neighbors = regionQuery(FFs, idx, 100);
                if(new_neighbors.size() >= 2)
                {
                    for(long unsigned int k = 0; k < new_neighbors.size(); k++)
                    {
                        if(std::find(neighbors.begin(), neighbors.end(), new_neighbors[k]) == neighbors.end())
                        {
                            neighbors.push_back(new_neighbors[k]);
                        }
                    }
                }
            }
            if(std::find(cluster.begin(), cluster.end(), FFs[idx]) == cluster.end())
            {
                cluster.push_back(FFs[idx]);
            }
        }
        clusters.push_back(cluster);
    }

    return clusters;
}

void Solver::findForceDirectedPlacementBankingFFs(FF* ff1, FF* ff2, int& result_x, int& result_y)
{
    double x_sum = 0.0;
    double y_sum = 0.0;
    int num_pins = 0;
    for (auto inPin : ff1->getInputPins())
    {
        Pin* fanin = inPin->getFaninPin();
        if (fanin->getCell() != ff2)
        {
            x_sum += fanin->getGlobalX();
            y_sum += fanin->getGlobalY();
            num_pins++;
        }
    }
    for (auto inPin : ff2->getInputPins())
    {
        Pin* fanin = inPin->getFaninPin();
        if (fanin->getCell() != ff1)
        {
            x_sum += fanin->getGlobalX();
            y_sum += fanin->getGlobalY();
            num_pins++;
        }
    }
    for (auto outPin : ff1->getOutputPins())
    {
        for (auto fanout : outPin->getFanoutPins())
        {
            if (fanout->getCell() != ff2)
            {
                x_sum += fanout->getGlobalX();
                y_sum += fanout->getGlobalY();
                num_pins++;
            }
        }
    }
    for (auto outPin : ff2->getOutputPins())
    {
        for (auto fanout : outPin->getFanoutPins())
        {
            if (fanout->getCell() != ff1)
            {
                x_sum += fanout->getGlobalX();
                y_sum += fanout->getGlobalY();
                num_pins++;
            }
        }
    }
    int x_avg = x_sum / num_pins;
    int y_avg = y_sum / num_pins;
    Site* nearest_site = _siteMap->getNearestSite(x_avg, y_avg);
    result_x = nearest_site->getX();
    result_y = nearest_site->getY();
}

double Solver::cal_banking_gain(FF* ff1, FF* ff2, LibCell* targetFF)
{
    if(ff1->getBit()+ff2->getBit()!=targetFF->bit){
        std::cout << "Invalid target FF" << std::endl;
        return -INT_MAX;
    }
    double gain = 0.0;
    gain += BETA * (ff1->getPower() + ff2->getPower() - targetFF->power);
    gain += GAMMA * (ff1->getArea() + ff2->getArea() - targetFF->width * targetFF->height);

    int target_x, target_y;
    findForceDirectedPlacementBankingFFs(ff1, ff2, target_x, target_y);
    const int ff1_bit = ff1->getBit();
    const int ff2_bit = ff2->getBit();
    // D pin
    for (int i = 0; i < ff1_bit+ff2_bit; i++)
    {
        int op_idx = (i < ff1_bit) ? i : i - ff1_bit;
        FF* workingFF = (i < targetFF->bit) ? ff1 : ff2;
        Pin* inPin = workingFF->getInputPins()[op_idx];
        Pin* mapInPin = targetFF->inputPins[op_idx];
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
            const int new_fanin_x = target_x + targetFF->outputPins[fanin_ff_pin_idx]->getX();
            const int new_fanin_y = target_y + targetFF->outputPins[fanin_ff_pin_idx]->getY();
            const int diff_dist = abs(target_x+mapInPin->getX()-new_fanin_x) + abs(target_y+mapInPin->getY()-new_fanin_y) - abs(inPin->getGlobalX()-old_fanin_x) - abs(inPin->getGlobalY()-old_fanin_y);
            const double old_slack = inPin->getSlack();
            const double new_slack = old_slack - DISP_DELAY * diff_dist;
            gain -= calDiffCost(old_slack, new_slack);
        }
        else
        {
            gain -= calCostMoveD(inPin, inPin->getGlobalX(), inPin->getGlobalY(), target_x + mapInPin->getX(), target_y + mapInPin->getY());
        }
    }
    // Q pin
    for (int i = 0; i < ff1_bit+ff2_bit; i++)
    {
        int op_idx = (i < ff1_bit) ? i : i - ff1_bit;
        FF* workingFF = (i < targetFF->bit) ? ff1 : ff2;
        Pin* outPin = workingFF->getOutputPins()[op_idx];
        Pin* mapOutPin = targetFF->outputPins[op_idx];
        gain += calCostChangeQDelay(outPin, targetFF->qDelay - workingFF->getQDelay());
        for (auto nextStagePin : outPin->getNextStagePins())
        {
            if (nextStagePin->getType() == PinType::FF_D)
            {
                if (nextStagePin->getCell() == ff1 || nextStagePin->getCell() == ff2)
                {
                    if (nextStagePin->getFaninPin()->getCell() == ff1 || nextStagePin->getFaninPin()->getCell() == ff2)
                    {
                        // already considered in the D pin
                        break;
                    }
                    const double next_old_slack = nextStagePin->getSlack();
                    std::vector<double> tempArrivalTimes = nextStagePin->getArrivalTimes();
                    std::vector<int> tempSortedCriticalIndex = nextStagePin->getSortedCriticalIndex();
                    const double old_arrival_time = tempArrivalTimes.at(tempSortedCriticalIndex.at(0));
                    // update the arrival time and re-sort the critical index
                    std::vector<int> indexList = nextStagePin->getPathIndex(outPin);
                    for (int index : indexList)
                    {
                        std::vector<Pin*> path = nextStagePin->getPathToPrevStagePins(index);
                        Pin* secondLastPin = path.at(path.size()-2);
                        const int secondLastPinX = secondLastPin->getGlobalX();
                        const int secondLastPinY = secondLastPin->getGlobalY();
                        const double old_arrival_time = tempArrivalTimes.at(index);
                        const double diff_arrival_time = DISP_DELAY * (abs(outPin->getGlobalX() - secondLastPinX) + abs(outPin->getGlobalY() - secondLastPinY) - abs(target_x + mapOutPin->getX() - secondLastPinX) - abs(target_y + mapOutPin->getY() - secondLastPinY));
                        const double new_arrival_time = old_arrival_time - diff_arrival_time;
                        tempArrivalTimes.at(index) = new_arrival_time;
                        // reheap the new arrival time to the sorted list
                        int sortIndex = 0;
                        while (tempSortedCriticalIndex.at(sortIndex) != index)
                        {
                            sortIndex++;
                        }
                        std::push_heap(tempSortedCriticalIndex.begin(), tempSortedCriticalIndex.begin()+sortIndex+1, [&tempArrivalTimes](int i, int j) {
                            return tempArrivalTimes.at(i) < tempArrivalTimes.at(j);
                        });
                    }
                    const double new_arrival_time = tempArrivalTimes.at(tempSortedCriticalIndex.at(0));
                    const double next_new_slack =  next_old_slack + (old_arrival_time - new_arrival_time);
                    gain -= calDiffCost(next_old_slack, next_new_slack);
                }
                else
                {
                    const double next_old_slack = nextStagePin->getSlack();
                    const double next_new_slack = nextStagePin->calSlack(outPin, outPin->getGlobalX(), outPin->getGlobalY(), target_x + mapOutPin->getX(), target_y + mapOutPin->getY());
                    gain -= calDiffCost(next_old_slack, next_new_slack);
                }
            }
        }
    }
    return gain;
}

void Solver::greedyBanking(std::vector<std::vector<FF*>> clusters)
{
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
                            double gain = cal_banking_gain(ff1, ff2, ff);
                            if(gain > maxGain || bestFF1 == nullptr)
                            {
                                maxGain = gain;
                                bestFF1 = ff1;
                                bestFF2 = ff2;
                                targetFF = ff;
                            }
                        }
                    }
                }
            }
            if (maxGain > 0)
            {
                bankFFs(bestFF1, bestFF2, targetFF);
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

void Solver::dump(std::string filename)
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
