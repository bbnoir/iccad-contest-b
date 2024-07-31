#pragma once
#include <string>
#include <queue>
#include "Cell.h"
#include "Net.h"
#include "param.h"

class Net;
class Cell;

// Pin type
enum class PinType
{
    INPUT,
    OUTPUT,
    GATE_IN,
    GATE_OUT,
    FF_D,
    FF_Q,
    FF_CLK
};

class Pin
{
    public:
        Pin(PinType type, int x, int y, std::string name, Cell* cell);
        ~Pin();
        
        std::string getName();
        std::string getOriginalName();
        std::vector<std::string> getOriginalNames();
        int getX();
        int getY();
        int getGlobalX();
        int getGlobalY();
        bool isDpin();
        double getSlack();
        Cell *getCell();
        PinType getType();
        Net* getNet();
        Pin* getFaninPin();
        Pin* getFirstFanoutPin();
        std::vector<Pin*> getFanoutPins();
        std::vector<Pin*> getPrevStagePins();
        size_t getPrevStagePinsSize();
        std::vector<Pin*> getPathToPrevStagePins(int idx);
        std::vector<Pin*> getNextStagePins();
        size_t getNextStagePinsSize();
        std::vector<double> getArrivalTimes();
        std::vector<double>& getArrivalTimesRef();
        std::vector<int> getSortedCriticalIndex();
        std::vector<int>& getSortedCriticalIndexRef();

        void setSlack(double slack);
        void setCell(Cell* cell);
        void setOriginalName();
        void setOriginalName(std::string ori_name);
        void addOriginalName(std::string ori_name);
        void setFaninPin(Pin* pin);
        void addFanoutPin(Pin* pin);
        void addPrevStagePin(Pin* pin, std::vector<Pin*> path);
        void addNextStagePin(Pin* pin);
        void initArrivalTime();
        void initCriticalIndex();
        std::vector<int> getPathIndex(Pin* prevStagePin);
        double calSlack(Pin* movedPrevStagePin, int sourceX, int sourceY, int targetX, int targetY);
        double calSlackQ(Pin* changeQPin, double diffQDelay);
        double updateSlack(Pin* movedPrevStagePin, int sourceX, int sourceY, int targetX, int targetY);
        double updateSlackQ(Pin* changeQPin, double diffQDelay);

        void connect(Net* net);
        void copyConnection(Pin* pin);
        void transInfo(Pin* pin);

    private:
        PinType _type;
        // Pin coordinates(relative to the cell)
        int _x;
        int _y;
        std::string _name;
        std::vector<std::string> _originalCellPinNames;
        // which cell this pin belongs to
        Cell* _cell;
        double _slack;
        double _initCriticalArrivalTime;
        bool _isDpin;
        // which net this pin is connected to
        Net* _net;
        Pin* _faninPin;
        std::vector<Pin*> _fanoutPins;

        // Previous stage pins
        std::vector<Pin*> _prevStagePins;
        std::vector<std::vector<Pin*>> _pathToPrevStagePins;
        std::vector<double> _arrivalTimes;
        std::vector<int> _sortedCriticalIndex;
        // Next stage pins
        std::vector<Pin*> _nextStagePins;

};