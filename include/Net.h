#pragma once
#include <vector>
#include <string>
#include "Pin.h"

class Pin;

class Net
{
    public:
        Net(std::string name, std::vector<Pin*> pins);
        ~Net();

        std::string getName();
        std::vector<Pin*> getPins();
        
    private:
        std::string name;
        std::vector<Pin*> pins;
};