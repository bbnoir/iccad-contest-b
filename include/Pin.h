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
    CELL
};

class Pin
{
    public:
        Pin(PinType type, int x, int y, std::string name, Cell* cell);
        ~Pin();
        
        std::string getName();
        int getX();
        int getY();
        bool isDpin();
        double getSlack();
        Cell *getCell();

        void setSlack(double slack);
        void setCell(Cell* cell);

        void connect(Net* net);

    private:
        PinType _type;
        // Pin coordinates(relative to the cell)
        int _x;
        int _y;
        std::string _name;
        // which cell this pin belongs to
        Cell* _cell;
        double _slack;
        bool _isDpin;
        // which net this pin is connected to
        Net* _net;

};