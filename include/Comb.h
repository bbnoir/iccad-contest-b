#pragma once
#include <string>
#include "Cell.h"
#include "Pin.h"

class Comb : public Cell
{
    public:
        using Cell::Cell;
        ~Comb();
        
        void addPin(Pin* pin);

    private:
        std::vector<Pin*> pins;
};