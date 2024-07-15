#pragma once
#include <string>
#include <algorithm>
#include "Site.h"
#include "Pin.h"

class Site;
class Pin;
class Bin;

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
    std::vector<Pin*> pins;
    std::vector<Pin*> inputPins;
    std::vector<Pin*> outputPins;
    Pin* clkPin;

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
    std::string getInstName();
    std::string getCellName();
    CellType getCellType();
    std::vector<Pin*> getPins();
    std::vector<Pin*> getInputPins();
    std::vector<Pin*> getOutputPins();
    Pin* getPin(std::string pin_name);
    std::vector<Site*> getSites();
    std::vector<Bin*> getBins();

    void setX(int x);
    void setY(int y);
    void setInstName(std::string inst_name);
    void addSite(Site* site);
    void addBin(Bin* bin);
    void addPin(Pin* pin);

    void removeSite(Site* site);
    void removeBin(Bin* bin);

protected:
    LibCell* _lib_cell;
    int _x;
    int _y;
    std::string _inst_name;
    std::vector<Pin*> _pins;
    std::vector<Pin*> _inputPins;
    std::vector<Pin*> _outputPins;
    Pin* _clkPin;
    // Site where the cell is placed
    std::vector<Site*> _sites;
    std::vector<Bin*> _bins;
};