#pragma once
#include <string>
#include "Site.h"
#include "Pin.h"

class Site;
class Pin;

// Cell type
enum class CellType
{
    FF,
    COMB,
};

struct LibCell
{
    CellType type;
    int width;
    int height;
    double power;
    double qDelay;
    int bit;
    std::string cell_name;
    std::vector<Pin> pins;

    LibCell()
    {
        type = CellType::COMB;
        width = 0;
        height = 0;
        power = 0.0;
        qDelay = 0.0;
        bit = 0;
        cell_name = "";
    }
    LibCell(CellType type, int width, int height, double power, double qDelay, int bit, std::string cell_name)
    {
        this->type = type;
        this->width = width;
        this->height = height;
        this->power = power;
        this->qDelay = qDelay;
        this->bit = bit;
        this->cell_name = cell_name;
    }
};

class Cell
{
public:
    Cell();
    // constructor for instance cells
    Cell(int x, int y, std::string inst_name, LibCell* lib_cell);
    ~Cell();

    int getX();
    int getY();
    int getWidth();
    int getHeight();
    int getArea();
    double getPower();
    std::string getInstName();
    std::string getCellName();
    std::vector<Pin*> getPins();
    Pin* getPin(std::string pin_name);

    void setX(int x);
    void setY(int y);
    void setInstName(std::string inst_name);
    void setSite(Site* site);

    void addPin(Pin* pin);

protected:
    LibCell* _lib_cell;
    int _x;
    int _y;
    std::string _inst_name;
    std::vector<Pin*> pins;
    // Site where the cell is placed
    Site* _site;
};