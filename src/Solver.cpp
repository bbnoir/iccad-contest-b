#include "Solver.h"
#include "Cell.h"
#include "Comb.h"
#include "FF.h"
#include "Net.h"
#include "Pin.h"
#include "Site.h"
#include "Bin.h"

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
}

Solver::~Solver()
{
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
            }else if(pin->getType() == PinType::FF_Q || pin->getType() == PinType::GATE_OUT)
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
        _ffsMap[instName]->getPin(port)->setSlack(slack);
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

    cout << "File parsed successfully" << endl;
    in.close();
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
        if(!placeCell(ff))
        {
            std::cerr << "Overlapped initial placement" << std::endl;
            exit(1);
        }
    }
    for (auto comb : _combs)
    {
        if(!placeCell(comb))
        {
            std::cerr << "Overlapped initial placement" << std::endl;
            exit(1);
        }
    }
}

bool Solver::placeCell(Cell* cell, bool allowOverlap)
{
    bool overlap = _siteMap->place(cell, allowOverlap);
    if(overlap && !allowOverlap)
    {
        // not allow 
        return false;
    }
    _binMap->addCell(cell);
    return true;
}

void Solver::removeCell(Cell* cell)
{
    _siteMap->removeCell(cell);
    _binMap->removeCell(cell);
}

bool Solver::moveCell(Cell* cell, int x, int y, bool allowOverlap)
{
    removeCell(cell);
    cell->setX(x);
    cell->setY(y);
    return placeCell(cell, allowOverlap);
}

void Solver::addFF(FF* ff)
{
    _ffs.push_back(ff);
    _ffsMap[ff->getInstName()] = ff;
}

void Solver::deleteFF(FF* ff)
{
    _ffs.erase(std::remove(_ffs.begin(), _ffs.end(), ff), _ffs.end());
    _ffsMap.erase(ff->getInstName());
    // TODO: fix delete FF also delete nets
    // delete ff;
}

void Solver::bankFFs(FF* ff1, FF* ff2, LibCell* targetFF)
{
    // calculate the new position
    int x = ff1->getX();
    int y = ff1->getY();
    // create a new FF
    FF* newFF = new FF(x, y, makeUniqueName("FF"), targetFF);
    addFF(newFF);
    // connect the pins
    std::vector<Pin*> pins1 = ff1->getPins();
    std::vector<Pin*> pins2 = ff2->getPins();
    std::vector<Pin*> newPins = newFF->getPins();
    // connect clk pin
    newPins[1]->connect(pins1[1]->getNet());
    pins1[1]->getNet()->addPin(newPins[1]);
    // connect d pin
    newPins[0]->connect(pins1[0]->getNet());
    newPins[3]->connect(pins2[0]->getNet());
    pins1[0]->getNet()->addPin(newPins[0]);
    pins2[0]->getNet()->addPin(newPins[3]);
    // connect q pin
    newPins[2]->connect(pins1[2]->getNet());
    newPins[4]->connect(pins2[2]->getNet());
    pins1[2]->getNet()->addPin(newPins[2]);
    pins2[2]->getNet()->addPin(newPins[4]);
    // remove the old pins
    for(auto pin : pins1)
    {
        pin->getNet()->removePin(pin);
    }
    for(auto pin : pins2)
    {
        pin->getNet()->removePin(pin);
    }
    // place the new FF
    placeCell(newFF, true);
    forceDirectedPlaceFF(newFF);
    // delete the old FFs
    removeCell(ff1);
    removeCell(ff2);
    deleteFF(ff1);
    deleteFF(ff2);
}

std::string Solver::makeUniqueName(std::string name)
{
    int i = 1;
    std::string newName = name;
    while(_ffsMap.find(newName) != _ffsMap.end() || _combsMap.find(newName) != _combsMap.end())
    {
        newName = name + std::to_string(i);
        i++;
    }
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
    for (auto ff : _ffs)
    {
        if (ff->getCellName() == _baseFF->cell_name)
        {
            debankedFFs.push_back(ff);
            continue;
        }
        std::vector<std::pair<Pin*, Pin*>> dqPairs = ff->getDQpairs();
        Pin* clkPin = ff->getClkPin();
        const int x = ff->getX();
        const int y = ff->getY();
        for (auto dq : dqPairs)
        {
            FF* newFF = new FF(x, y, makeUniqueName("FF"), _baseFF, dq, clkPin);
            debankedFFs.push_back(newFF);
        }
    }
    _ffs.clear();
    for (auto ff : debankedFFs)
    {
        addFF(ff);
        placeCell(ff, true);
    }
}

void Solver::forceDirectedPlaceFF(FF* ff)
{
    // calculate the force
    double x_sum = 0.0;
    double y_sum = 0.0;
    int num_pins = 0;
    for(auto pin : ff->getPins())
    {
        Net* net = pin->getNet();
        for(auto other_pin : net->getPins())
        {
            if(other_pin == pin)
                continue;
            x_sum += other_pin->getGlobalX();
            y_sum += other_pin->getGlobalY();
            num_pins++;
        }
    }
    int x_avg = x_sum / num_pins;
    int y_avg = y_sum / num_pins;
    // adjust it to fit the placement rows
    Site* nearest_site = _siteMap->getNearestSite(x_avg, y_avg);
    // move the ff
    moveCell(ff, nearest_site->getX(), nearest_site->getY(), true);
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
    Pin* clkPin = ff->getClkPin()->getFaninPin();
    x_sum += clkPin->getGlobalX();
    y_sum += clkPin->getGlobalY();
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
    double dist = sqrt(pow(nearest_site->getX() - ff->getX(), 2) + pow(nearest_site->getY() - ff->getY(), 2));
    moveCell(ff, nearest_site->getX(), nearest_site->getY(), true);
    if (dist == 0)
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
    std::cout << "Force directed placement" << std::endl;
    bool converged = false;
    int max_iter = 1000;
    int iter = 0;
    const int numFFs = _ffs.size();
    const int lockThreshold = numFFs;
    std::cout << "Lock threshold: " << lockThreshold << std::endl;
    int lock_num = 0;
    std::vector<char> lock_cnt(numFFs, 0);
    std::vector<bool> locked(numFFs, false);
    while(!converged && iter++ < max_iter){
        for (int i = 0; i < numFFs; i++)
        {
            forceDirectedPlaceFFLock(i, locked, lock_cnt, lock_num);
        }
        std::cout << "Iteration: " << iter << ", Lock/Total: " << lock_num << "/" << numFFs << std::endl;
        converged = lock_num >= lockThreshold;
    }
}

double Solver::cal_total_hpwl()
{
    double hpwl = 0.0;
    for(auto net : _nets)
    {
        int minX = INT_MAX;
        int minY = INT_MAX;
        int maxX = INT_MIN;
        int maxY = INT_MIN;
        for(auto pin : net->getPins())
        {
            int x = pin->getGlobalX();
            int y = pin->getGlobalY();
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
        hpwl += (maxX - minX) + (maxY - minY);
    }
    return hpwl;
}

void Solver::solve()
{
    chooseBaseFF();
    init_placement();
    debankAll();
    forceDirectedPlacement();
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
        cout << "Power: " << comb->getPower() << endl;
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