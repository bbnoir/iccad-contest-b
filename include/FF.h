#pragma once
#include <string>
#include "Cell.h"
#include "Pin.h"

class FF : public Cell
{
    public:
        using Cell::Cell;
        ~FF();

        void addPin(Pin* pin);
        
    private:
        std::vector<Pin*> pins;
        int _bit;
        double _qDelay;
        double _power;
};