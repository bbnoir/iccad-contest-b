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
    _initSlack = 0;
}

Pin::~Pin()
{
    for (auto pathIndexList : _prevPathIndexListMap)
    {
        delete pathIndexList.second;
    }
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
}

void Pin::setInitSlack(double initSlack)
{
    _slack = initSlack;
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

void Pin::copyConnection(Pin* pin)
{
    _faninPin = pin->getFaninPin();
    _fanoutPins = pin->getFanoutPins();
}

void Pin::transInfo(Pin* pin)
{
    _x = pin->getX();
    _y = pin->getY();
    _name = pin->getName();
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
void Pin::resetArrivalTime(bool check)
{
    std::vector<double> tempArrivalTimes;
    if (check)
    {
        tempArrivalTimes = _arrivalTimes;
    }
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
    for (size_t i = 0; i < _arrivalTimes.size(); i++)
    {
        if (_arrivalTimes.at(i) > _currCriticalArrivalTime)
        {
            _currCriticalArrivalTime = _arrivalTimes.at(i);
            _currCriticalIndex = i;
        }
    }
}

/*
Calculate the slack of this pin after a previous stage pin is moved, no update
Return the calculated slack
*/
double Pin::calSlack(Pin* movedPrevStagePin, int sourceX, int sourceY, int targetX, int targetY, bool update)
{
    if (this->getType() != PinType::FF_D)
    {
        return _slack;
    }
    if (_arrivalTimes.size() == 0)
    {
        return _slack;
    }
    std::vector<double>* tempArrivalTimes = (update) ? &_arrivalTimes : new std::vector<double>(_arrivalTimes);
    // update the arrival time and re-sort the critical index
    const double old_critical_arrival_time = _currCriticalArrivalTime;
    double* new_critical_arrival_time = (update) ? &_currCriticalArrivalTime : new double(_currCriticalArrivalTime);
    size_t* new_critical_index = (update) ? &_currCriticalIndex : new size_t(_currCriticalIndex);
    std::vector<size_t>* indexList = getPathIndex(movedPrevStagePin);
    for (size_t index : *indexList)
    {
        std::vector<Pin*> path = _pathToPrevStagePins.at(index);
        Pin* secondLastPin = path.at(path.size()-2);
        const int secondLastPinX = secondLastPin->getGlobalX();
        const int secondLastPinY = secondLastPin->getGlobalY();
        const double diff_arrival_time = (abs(sourceX - secondLastPinX) + abs(sourceY - secondLastPinY) - abs(targetX - secondLastPinX) - abs(targetY - secondLastPinY)) * DISP_DELAY;
        tempArrivalTimes->at(index) -= diff_arrival_time;
        if (tempArrivalTimes->at(index) > *new_critical_arrival_time)
        {
            *new_critical_arrival_time = tempArrivalTimes->at(index);
            *new_critical_index = index;
        }
        else if (index == *new_critical_index && tempArrivalTimes->at(index) < *new_critical_arrival_time)
        {
            *new_critical_arrival_time = tempArrivalTimes->at(index);
            for (size_t i = 0; i < tempArrivalTimes->size(); i++)
            {
                if (tempArrivalTimes->at(i) > *new_critical_arrival_time)
                {
                    *new_critical_arrival_time = tempArrivalTimes->at(i);
                    *new_critical_index = i;
                }
            }
        }
    }
    const double new_slack = _slack + (old_critical_arrival_time - *new_critical_arrival_time);
    if (update)
    {
        _slack = new_slack;
    }
    else
    {
        delete tempArrivalTimes;
        delete new_critical_arrival_time;
        delete new_critical_index;
    }
    return new_slack;
}

/*
Calculate the slack of this pin after the Q delay of a previous stage pin is changed, no update
Return the calculated slack
*/
double Pin::calSlackQ(Pin* changeQPin, double diffQDelay, bool update)
{
    if (this->getType() != PinType::FF_D)
    {
        return _slack;
    }
    if (_arrivalTimes.size() == 0)
    {
        return _slack;
    }
    std::vector<double>* tempArrivalTimes = (update) ? &_arrivalTimes : new std::vector<double>(_arrivalTimes);
    const double old_arrival_time = _currCriticalArrivalTime;
    double* new_arrival_time = (update) ? &_currCriticalArrivalTime : new double(_currCriticalArrivalTime);
    size_t* new_critical_index = (update) ? &_currCriticalIndex : new size_t(_currCriticalIndex);
    std::vector<size_t>* indexList = getPathIndex(changeQPin);
    for (size_t index : *indexList)
    {
        tempArrivalTimes->at(index) += diffQDelay;
        if (tempArrivalTimes->at(index) > *new_arrival_time)
        {
            *new_arrival_time = tempArrivalTimes->at(index);
            *new_critical_index = index;
        }
        else if (index == *new_critical_index && tempArrivalTimes->at(index) < *new_arrival_time)
        {
            *new_arrival_time = tempArrivalTimes->at(index);
            for (size_t i = 0; i < tempArrivalTimes->size(); i++)
            {
                if (tempArrivalTimes->at(i) > *new_arrival_time)
                {
                    *new_arrival_time = tempArrivalTimes->at(i);
                    *new_critical_index = i;
                }
            }
        }
    }
    const double new_slack = _slack + (old_arrival_time - *new_arrival_time);
    if (update)
    {
        _slack = new_slack;
    }
    else
    {
        delete tempArrivalTimes;
        delete new_arrival_time;
        delete new_critical_index;
    }
    return new_slack;
}

/*
Reset the slack and the arrival time of this pin
*/
void Pin::resetSlack(bool check)
{
    if (_arrivalTimes.size() == 0)
    {
        _slack = (_initSlack == 0) ? 0 : _initSlack;
    }
    else
    {
        if (!check)
        {
            this->resetArrivalTime(false);
            _slack = _initSlack + (_initCriticalArrivalTime - _currCriticalArrivalTime);
        }
    }
}

void Pin::initPathMaps()
{
    for (size_t i = 0; i < _pathToPrevStagePins.size(); i++)
    {
        Pin* prevPin = _prevStagePins.at(i);
        if (_prevPathIndexListMap.find(prevPin) == _prevPathIndexListMap.end())
        {
            _prevPathIndexListMap[prevPin] = new std::vector<size_t>();
        }
        _prevPathIndexListMap[prevPin]->push_back(i);
    }
}

void Pin::modArrivalTime(double delay)
{
    for (size_t i = 0; i < _arrivalTimes.size(); i++)
    {
        _arrivalTimes.at(i) += delay;
    }
    _currCriticalArrivalTime += delay;
}