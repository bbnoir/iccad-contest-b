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
        double getPower();

        void setBit(int bit);
        void setQDelay(double qDelay);
        void setPower(double power);

    private:
        
        int _bit;
        double _qDelay;
        double _power;
};