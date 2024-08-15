#include "Pin.h"
#include <iostream>
#include <algorithm>

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

int Pin::getGlobalX() const
{
    return (_cell != nullptr) ? _cell->getX() + _x : _x;
}

int Pin::getGlobalY() const
{
    return (_cell != nullptr) ? _cell->getY() + _y : _y;
}

void Pin::setSlack(double slack)
{
    _slack = slack;
    _isDpin = true;
}

void Pin::setInitSlack(double initSlack)
{
    _slack = initSlack;
    _isDpin = true;
    _initSlack = initSlack;
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

void Pin::addNextStagePin(Pin* pin, std::vector<Pin*> path)
{
    _nextStagePins.push_back(pin);
    _pathToNextStagePins.push_back(std::vector<Pin*>(path));
}

void Pin::connect(Net* net)
{
    _net = net;
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
    resetArrivalTime();
    _initCriticalArrivalTime = _currCriticalArrivalTime;
}

/*
Reset the arrival time of all paths from previous stage pins to this pin
*/
void Pin::resetArrivalTime()
{
    _arrivalTimes.clear();
    const size_t numPaths = _prevStagePins.size();
    for (size_t i = 0; i < numPaths; i++)
    {
        std::vector<Pin*> path = _pathToPrevStagePins.at(i);
        double arrival_time = 0;
        for (size_t j = 0; j+1 < path.size(); j+=2)
        {
            Pin* curPin = path.at(j);
            Pin* prevPin = path.at(j+1);
            arrival_time += abs(curPin->getGlobalX() - prevPin->getGlobalX()) + abs(curPin->getGlobalY() - prevPin->getGlobalY());
        }
        arrival_time *= DISP_DELAY;
        if (path.back()->getType() == PinType::FF_Q)
        {
            arrival_time += path.back()->getCell()->getQDelay();
        }
        _arrivalTimes.push_back(arrival_time);
    }
    _currCriticalArrivalTime = 0;
    for (auto arrival_time : _arrivalTimes)
    {
        _currCriticalArrivalTime = std::max(_currCriticalArrivalTime, arrival_time);
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
Calculate the slack of this pin after a previous stage pin is moved, no update
Return the calculated slack
*/
double Pin::calSlack(Pin* movedPrevStagePin, int sourceX, int sourceY, int targetX, int targetY)
{
    if (this->getType() != PinType::FF_D)
    {
        std::cerr << "Error: only D pin can update slack" << std::endl;
        exit(1);
    }
    std::vector<double> tempArrivalTimes = _arrivalTimes;
    const double old_arrival_time = _currCriticalArrivalTime;
    // update the arrival time and re-sort the critical index
    std::vector<int> indexList = getPathIndex(movedPrevStagePin);
    for (int index : indexList)
    {
        std::vector<Pin*> path = _pathToPrevStagePins.at(index);
        Pin* secondLastPin = path.at(path.size()-2);
        const int secondLastPinX = secondLastPin->getGlobalX();
        const int secondLastPinY = secondLastPin->getGlobalY();
        const double old_arrival_time = tempArrivalTimes.at(index);
        const double diff_arrival_time = (abs(sourceX - secondLastPinX) + abs(sourceY - secondLastPinY) - abs(targetX - secondLastPinX) - abs(targetY - secondLastPinY)) * DISP_DELAY;
        const double new_arrival_time = old_arrival_time - diff_arrival_time;
        tempArrivalTimes.at(index) = new_arrival_time;
    }
    double new_arrival_time = 0;
    for (double arrival_time : tempArrivalTimes)
    {
        new_arrival_time = std::max(new_arrival_time, arrival_time);
    }
    return _slack + (old_arrival_time - new_arrival_time);
}

/*
Update the slack of this pin after a previous stage pin is moved
Return the new slack
*/
double Pin::updateSlack(Pin* movedPrevStagePin, int sourceX, int sourceY, int targetX, int targetY)
{
    if (this->getType() != PinType::FF_D)
    {
        std::cerr << "Error: only D pin can update slack" << std::endl;
        exit(1);
    }
    const double old_arrival_time = _currCriticalArrivalTime;
    // update the arrival time and re-sort the critical index
    std::vector<int> indexList = getPathIndex(movedPrevStagePin);
    for (int index : indexList)
    {
        std::vector<Pin*> path = _pathToPrevStagePins.at(index);
        Pin* secondLastPin = path.at(path.size()-2);
        const int secondLastPinX = secondLastPin->getGlobalX();
        const int secondLastPinY = secondLastPin->getGlobalY();
        const double old_arrival_time = _arrivalTimes.at(index);
        const double diff_arrival_time = (abs(sourceX - secondLastPinX) + abs(sourceY - secondLastPinY) - abs(targetX - secondLastPinX) - abs(targetY - secondLastPinY)) * DISP_DELAY;
        const double new_arrival_time = old_arrival_time - diff_arrival_time;
        _arrivalTimes.at(index) = new_arrival_time;
    }
    // update slack
    _currCriticalArrivalTime = 0;
    for (auto arrival_time : _arrivalTimes)
    {
        _currCriticalArrivalTime = std::max(_currCriticalArrivalTime, arrival_time);
    }
    const double new_arrival_time = _currCriticalArrivalTime;
    _slack += (old_arrival_time - new_arrival_time);
    return _slack;
}

/*
Calculate the slack of this pin after the Q delay of a previous stage pin is changed, no update
Return the calculated slack
*/
double Pin::calSlackQ(Pin* changeQPin, double diffQDelay)
{
    if (this->getType() != PinType::FF_D)
    {
        std::cerr << "Error: only D pin can update slack" << std::endl;
        exit(1);
    }
    std::vector<double> tempArrivalTimes = _arrivalTimes;
    const double old_arrival_time = _currCriticalArrivalTime;
    std::vector<int> indexList = getPathIndex(changeQPin);
    for (int index : indexList)
    {
        tempArrivalTimes.at(index) += diffQDelay;
    }
    double new_arrival_time = 0;
    for (double arrival_time : tempArrivalTimes)
    {
        new_arrival_time = std::max(new_arrival_time, arrival_time);
    }
    return _slack + (old_arrival_time - new_arrival_time);
}

/*
Update the slack of this pin after the Q delay of a previous stage pin is changed
Return the new slack
*/
double Pin::updateSlackQ(Pin* changeQPin, double diffQDelay)
{
    if (this->getType() != PinType::FF_D)
    {
        std::cerr << "Error: only D pin can update slack" << std::endl;
        exit(1);
    }
    const double old_arrival_time = _currCriticalArrivalTime;
    std::vector<int> indexList = getPathIndex(changeQPin);
    for (int index : indexList)
    {
        _arrivalTimes.at(index) += diffQDelay;
    }
    _currCriticalArrivalTime = 0;
    for (auto arrival_time : _arrivalTimes)
    {
        _currCriticalArrivalTime = std::max(_currCriticalArrivalTime, arrival_time);
    }
    const double new_arrival_time = _currCriticalArrivalTime;
    _slack += (old_arrival_time - new_arrival_time);
    return _slack;
}

/*
Reset the slack and the arrival time of this pin
*/
void Pin::resetSlack(bool check)
{
    if (_arrivalTimes.size() == 0)
    {
        // TODO: there is empty path pin
        _slack = (_initSlack == 0) ? 0 : _initSlack;
    }
    else
    {
        if (!check)
        {
            this->resetArrivalTime();
            _slack = _initSlack + (_initCriticalArrivalTime - _currCriticalArrivalTime);
        }
        else
        {
            double old_slack = _slack;
            this->resetArrivalTime();
            _slack = _initSlack + (_initCriticalArrivalTime - _currCriticalArrivalTime);
            if (std::abs(old_slack - _slack) > 1e-6)
            {
                std::cout << "Warning: slack after reset is not the same as before" << std::endl;
                std::cout << "Pin: " << this->getCell()->getInstName() << "/" << this->getName() << std::endl;
                std::cout << "Old slack: " << old_slack << std::endl;
                std::cout << "New slack: " << _slack << std::endl;
                std::cout << "Difference: " << old_slack - _slack << std::endl;
            }
        }
    }
}

/*
Check if _currCriticalArrivalTime is really the critical arrival time
*/
bool Pin::checkCritical()
{
    double max_arrival_time = 0;
    for (double arrival_time : _arrivalTimes)
    {
        max_arrival_time = std::max(max_arrival_time, arrival_time);
    }
    return max_arrival_time == _currCriticalArrivalTime;
}