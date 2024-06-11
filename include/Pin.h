#pragma once
#include <string>
#include "Cell.h"
#include "Net.h"

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

        void setSlack(double slack);
        void setCell(Cell* cell);
        void setOriginalName();
        void setOriginalName(std::string ori_name);
        void setFaninPin(Pin* pin);
        void addFanoutPin(Pin* pin);

        void connect(Net* net);
        void copyConnection(Pin* pin);

    private:
        PinType _type;
        // Pin coordinates(relative to the cell)
        int _x;
        int _y;
        std::string _name;
        std::string _originalCellPinName;
        // which cell this pin belongs to
        Cell* _cell;
        double _slack;
        bool _isDpin;
        // which net this pin is connected to
        Net* _net;
        Pin* _faninPin;
        std::vector<Pin*> _fanoutPins;

};