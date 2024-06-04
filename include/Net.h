#pragma once
#include <vector>
#include <string>
#include "Pin.h"

class Pin;

class Net
{
    public:
        Net(std::string name);
        Net(std::string name, std::vector<Pin*> pins);
        ~Net();

        std::string getName();
        std::vector<Pin*> getPins();

        void setPins(std::vector<Pin*> pins);
        void addPin(Pin* pin);
        void removePin(Pin* pin);
        
    private:
        std::string name;
        std::vector<Pin*> pins;
};