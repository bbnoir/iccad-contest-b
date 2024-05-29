#pragma once
#include <string>
#include "Cell.h"
#include "Net.h"

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
        
        void connect(Net* net);

    private:
        // Pin coordinates(relative to the cell)
        PinType _type;
        int _x;
        int _y;
        std::string _name;
        Cell* _cell;
        Net* _net;
};