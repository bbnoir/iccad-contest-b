#include "Pin.h"
#include <iostream>

Pin::Pin(PinType type, int x, int y, std::string name, Cell* cell)
{
    _type = type;
    _x = x;
    _y = y;
    _name = name;
    _cell = cell;
    _slack = 0;
    _isDpin = false;
}

Pin::~Pin()
{
}

std::string Pin::getName()
{
    return _name;
}

std::string Pin::getOriginalName()
{
    return _originalCellPinNames[0];
}

std::vector<std::string> Pin::getOriginalNames()
{
    return _originalCellPinNames;
}

int Pin::getX()
{
    return _x;
}

int Pin::getY()
{
    return _y;
}

int Pin::getGlobalX()
{
    if (_cell != nullptr)
    {
        return _cell->getX() + _x;
    }
    return _x;
}

int Pin::getGlobalY()
{
    if (_cell != nullptr)
    {
        return _cell->getY() + _y;
    }
    return _y;
}

bool Pin::isDpin()
{
    return _isDpin;
}

double Pin::getSlack()
{
    return _slack;
}

Cell* Pin::getCell()
{
    return _cell;
}

PinType Pin::getType()
{
    return _type;
}

Net* Pin::getNet()
{
    return _net;
}

void Pin::setSlack(double slack)
{
    _slack = slack;
    _isDpin = true;
}

void Pin::setCell(Cell* cell)
{
    _cell = cell;
}

void Pin::setOriginalName()
{
    _originalCellPinNames.clear();
    _originalCellPinNames.push_back(this->getCell()->getInstName() + "/" + this->getName());
}

void Pin::setOriginalName(std::string ori_name)
{
    _originalCellPinNames.clear();
    _originalCellPinNames.push_back(ori_name);
}

void Pin::addOriginalName(std::string ori_name)
{
    _originalCellPinNames.push_back(ori_name);
}

void Pin::setFaninPin(Pin* pin)
{
    _faninPin = pin;
}

void Pin::addFanoutPin(Pin* pin)
{
    _fanoutPins.push_back(pin);
}

void Pin::addPrevStagePin(Pin* pin, std::vector<Pin*> path)
{
    _prevStagePins.push_back(pin);
    _pathToPrevStagePins.push_back(std::vector<Pin*>(path));
}

void Pin::addNextStagePin(Pin* pin)
{
    _nextStagePins.push_back(pin);
}

void Pin::connect(Net* net)
{
    _net = net;
}

Pin* Pin::getFaninPin()
{
    return _faninPin;
}

Pin* Pin::getFirstFanoutPin()
{
    if (_fanoutPins.size() > 0)
    {
        return _fanoutPins[0];
    }
    return nullptr;
}

std::vector<Pin*> Pin::getFanoutPins()
{
    return _fanoutPins;
}

std::vector<Pin*> Pin::getPrevStagePins()
{
    return _prevStagePins;
}

size_t Pin::getPrevStagePinsSize()
{
    return _prevStagePins.size();
}

std::vector<Pin*> Pin::getPathToPrevStagePins(int idx)
{
    return _pathToPrevStagePins.at(idx);
}

std::vector<Pin*> Pin::getNextStagePins()
{
    return _nextStagePins;
}

size_t Pin::getNextStagePinsSize()
{
    return _nextStagePins.size();
}

void Pin::copyConnection(Pin* pin)
{
    _net = pin->getNet();
    _faninPin = pin->getFaninPin();
    _fanoutPins = pin->getFanoutPins();
}

void Pin::transInfo(Pin* pin)
{
    _x = pin->getX();
    _y = pin->getY();
    _name = pin->getName();
    _isDpin = pin->isDpin();
    _type = pin->getType();
}

/*
Initialize the arrival time of all paths from previous stage pins to this pin
*/
void Pin::initArrivalTime()
{
    const int numPaths = _prevStagePins.size();
    for (int i = 0; i < numPaths; i++)
    {
        std::vector<Pin*> path = _pathToPrevStagePins.at(i);
        double arrival_time = 0;
        for (size_t j = 0; j+1 < path.size(); j+=2)
        {
            Pin* curPin = path.at(j);
            Pin* prevPin = path.at(j+1);
            arrival_time += abs(curPin->getGlobalX() - prevPin->getGlobalX()) + abs(curPin->getGlobalY() - prevPin->getGlobalY());
        }
        // don't add q pin delay since it may change
        // arrival_time += path.back()->getCell()->getQDelay();
        _arrivalTimes.push_back(arrival_time);
        _sortedCriticalIndex.push_back(i);
    }
}

/*
Sort the index of paths by arrival time, critical path first
*/
void Pin::sortCriticalIndex()
{
    if (_sortedCriticalIndex.size() > 0)
    {
        std::sort(_sortedCriticalIndex.begin(), _sortedCriticalIndex.end(), [this](int i, int j) {
            return _arrivalTimes.at(i) > _arrivalTimes.at(j);
        });
        _initCriticalArrivalTime = _arrivalTimes.at(_sortedCriticalIndex.at(0));
    }
}

/*
Get the index of the path from previous stage pin to this pin
*/
std::vector<int> Pin::getPathIndex(Pin* prevStagePin)
{
    // TODO: change to unordered_map if it's slow
    std::vector<int> index;
    for (size_t i = 0; i < _prevStagePins.size(); i++)
    {
        if (_prevStagePins.at(i) == prevStagePin)
        {
            index.push_back(i);
        }
    }
    return index;
}

/*
Update the slack of this pin after a previous stage pin is moved
Return the new slack
*/
double Pin::updateSlack(Pin* movedPrevStagePin, int sourceX, int sourceY, int targetX, int targetY)
{
    if (this->getType() != PinType::FF_D)
    {
        std::cout << "Error: only D pin can update slack" << std::endl;
        exit(1);
    }
    const double old_arrival_time_diff = _initCriticalArrivalTime - _arrivalTimes.at(_sortedCriticalIndex.at(0));
    // update the arrival time and re-sort the critical index
    std::vector<int> indexList = getPathIndex(movedPrevStagePin);
    for (int index : indexList)
    {
        std::vector<Pin*> path = _pathToPrevStagePins.at(index);
        Pin* secondLastPin = path.at(path.size()-2);
        const int secondLastPinX = secondLastPin->getGlobalX();
        const int secondLastPinY = secondLastPin->getGlobalY();
        const double old_arrival_time = _arrivalTimes.at(index);
        const double new_arrival_time = old_arrival_time - abs(sourceX - secondLastPinX) - abs(sourceY - secondLastPinY) + abs(targetX - secondLastPinX) + abs(targetY - secondLastPinY);
        _arrivalTimes.at(index) = new_arrival_time;
        // insert the new arrival time to the sorted list
        int sortIndex = 0;
        while (_sortedCriticalIndex.at(sortIndex) != index)
        {
            sortIndex++;
        }
        while (sortIndex > 0 && _arrivalTimes.at(_sortedCriticalIndex.at(sortIndex-1)) < new_arrival_time)
        {
            _sortedCriticalIndex.at(sortIndex) = _sortedCriticalIndex.at(sortIndex-1);
            sortIndex--;
        }
        _sortedCriticalIndex.at(sortIndex) = index;
    }
    // update slack
    const double new_arrival_time_diff = _initCriticalArrivalTime - _arrivalTimes.at(_sortedCriticalIndex.at(0));
    _slack += (new_arrival_time_diff - old_arrival_time_diff) * DISP_DELAY;
    return _slack;
}