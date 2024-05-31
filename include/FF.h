#pragma once
#include <string>
#include "Cell.h"
#include "Pin.h"

class FF : public Cell
{
    public:
        using Cell::Cell;
        ~FF();

        int getBit();
        double getQDelay();
    private:
};