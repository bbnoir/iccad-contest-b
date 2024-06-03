#include "Solver.h"
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
int BIN_MAX_UTIL;
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
                ff->pins.push_back(Pin(PinType::CELL, x, y, pinName, nullptr));
            }
            _ffsLibList.push_back(ff);
            _ffsLibMap[name] = ff;
        }else if(token == "Gate"){
            int width, height, pinCount;
            string name;
            in >> name >> width >> height >> pinCount;
            LibCell* comb = new LibCell(CellType::COMB, width, height, 0.0, 0.0, 0, name);
            for(int i = 0; i < pinCount; i++)
            {
                string pinName;
                int x, y;
                in >> token >> pinName >> x >> y;
                comb->pins.push_back(Pin(PinType::CELL, x, y, pinName, nullptr));
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
    while(!in.eof())
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
        int x1 = ff->getX();
        int y1 = ff->getY();
        int x2 = x1 + ff->getWidth();
        int y2 = y1 + ff->getHeight();
        std::vector<Site*> sites = _siteMap->getSites(x1, y1, x2, y2);
        for(auto site : sites)
        {
            if(site->isOccupied())
            {
                std::cerr << "Overlapped initial placement" << std::endl;
                exit(1);
            }
            else
            {
                site->place(ff);
                ff->addSite(site);
            }
                
        }
        std::vector<Bin*> bins = _binMap->getBins(x1, y1, x2, y2);
        for (auto bin : bins)
        {
            bin->addCell(ff);
        }
    }
    for (auto comb : _combs)
    {
        int x1 = comb->getX();
        int y1 = comb->getY();
        int x2 = x1 + comb->getWidth();
        int y2 = y1 + comb->getHeight();
        std::vector<Site*> sites = _siteMap->getSites(x1, y1, x2, y2);
        for(auto site : sites)
        {
            if(site->isOccupied())
            {
                std::cerr << "Overlapped initial placement" << std::endl;
                exit(1);
            }
            else
            {
                site->place(comb);
                comb->addSite(site);
            }
                
        }
        std::vector<Bin*> bins = _binMap->getBins(x1, y1, x2, y2);
        for (auto bin : bins)
        {
            bin->addCell(comb);
        }
    }
}

void Solver::solve()
{
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
            cout << "Pin Name: " << pin.getName() << ", X: " << pin.getX() << ", Y: " << pin.getY() << endl;
        }
    }
    cout << "Gates libs:" << endl;
    for(auto comb : _combsLibList)
    {
        cout << "Name: " << comb->cell_name << ", Width: " << comb->width << ", Height: " << comb->height << endl;
        cout << "Power: " << comb->power << endl;
        for(auto pin : comb->pins)
        {
            cout << "Pin Name: " << pin.getName() << ", X: " << pin.getX() << ", Y: " << pin.getY() << endl;
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
}