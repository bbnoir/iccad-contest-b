#pragma once

class Cell
{
public:
    Cell();
    Cell(int x, int y);
    ~Cell();

    int getX();
    int getY();

    void setX(int x);
    void setY(int y);

private:
    int x;
    int y;
};