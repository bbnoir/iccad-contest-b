#include <iostream>
#include "Cell.h"

int main()
{
    Cell cell1;
    Cell cell2(1, 2);

    std::cout << "Cell 1: (" << cell1.getX() << ", " << cell1.getY() << ")" << std::endl;
    std::cout << "Cell 2: (" << cell2.getX() << ", " << cell2.getY() << ")" << std::endl;

    return 0;
}