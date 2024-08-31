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
        
        inline std::string getName() const { return _name; }
        inline std::string getOriginalName() const { return _originalCellPinNames[0]; }
        inline std::vector<std::string> getOriginalNames() const { return _originalCellPinNames; }
        inline int getX() const { return _x; }
        inline int getY() const { return _y; }
        int getGlobalX() const;
        int getGlobalY() const;
        inline bool isDpin() const { return _isDpin; }
        inline double getSlack() const { return _slack; }
        inline Cell *getCell() const { return _cell; }
        inline PinType getType() const { return _type; }
        inline Net* getNet() const { return _net; }
        inline Pin* getFaninPin() const { return _faninPin; }
        inline Pin* getFirstFanoutPin() const { return (_fanoutPins.size() > 0) ? _fanoutPins[0] : nullptr; }
        inline std::vector<Pin*> getFanoutPins() const { return _fanoutPins; }
        inline std::vector<Pin*> getPrevStagePins() const { return _prevStagePins; }
        inline size_t getPrevStagePinsSize() const { return _prevStagePins.size(); }
        inline std::vector<Pin*> getPathToPrevStagePins(int idx) const { return _pathToPrevStagePins.at(idx); }
        inline std::vector<Pin*> getNextStagePins() const { return _nextStagePins; }
        inline size_t getNextStagePinsSize() const { return _nextStagePins.size(); }
        inline const std::vector<std::vector<Pin*>>& getPathToNextStagePins() const { return _pathToNextStagePins; }
        inline std::vector<double> getArrivalTimes() const { return _arrivalTimes; }
        inline std::vector<double>& getArrivalTimesRef() { return _arrivalTimes; }

        void setSlack(double slack);
        void setInitSlack(double initSlack);
        void setCell(Cell* cell);
        void setOriginalName();
        void setOriginalName(std::string ori_name);
        void addOriginalName(std::string ori_name);
        void setFaninPin(Pin* pin);
        void addFanoutPin(Pin* pin);
        void addPrevStagePin(Pin* pin, std::vector<Pin*> path);
        void addNextStagePin(Pin* pin, std::vector<Pin*> path);
        void initArrivalTime();
        void resetArrivalTime(bool check = false);
        void modArrivalTime(double delay); // only for FF_D in debug mode
        inline std::vector<size_t>* getPathIndex(Pin* prevStagePin) { return _prevPathIndexListMap[prevStagePin]; }
        double calSlack(Pin* movedPrevStagePin, int sourceX, int sourceY, int targetX, int targetY, bool update = false);
        double calSlackQ(Pin* changeQPin, double diffQDelay, bool update = false);
        void resetSlack(bool check = false);
        bool checkCritical();
        void initPathMaps();

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
        bool _isDpin;
        // which net this pin is connected to
        Net* _net;
        Pin* _faninPin;
        std::vector<Pin*> _fanoutPins;

        // Previous stage pins
        std::vector<Pin*> _prevStagePins;
        // Next stage pins
        std::vector<Pin*> _nextStagePins;

        // for slack calculation
        size_t _currCriticalIndex;
        double _initSlack;
        double _initCriticalArrivalTime;
        double _currCriticalArrivalTime;
        std::vector<std::vector<Pin*>> _pathToPrevStagePins;
        std::vector<std::vector<Pin*>> _pathToNextStagePins;
        std::vector<double> _arrivalTimes;
        std::unordered_map<Pin*, std::vector<size_t>*> _prevPathIndexListMap;
};