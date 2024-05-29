#pragma once
#include <vector>
#include "Pin.h"

class Net
{
    public:
        explicit Net(const std::vector<Pin*>& pins);
        ~Net();
        
    private:
        std::vector<Pin*> pins;
};