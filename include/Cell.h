#pragma once
#include <string>
#include <algorithm>
#include "Bin.h"
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
    double costPA;

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

    int getX() const { return _x; }
    int getY() const { return _y; }
    int getWidth() const { return _lib_cell->width; }
    int getHeight() const { return _lib_cell->height; }
    int getArea() const { return _lib_cell->width * _lib_cell->height; }
    std::string getInstName() const { return _inst_name; }
    std::string getCellName() const { return _lib_cell->cell_name; }
    CellType getCellType() const { return _lib_cell->type; }
    std::vector<Pin*> getPins() const { return _pins; }
    std::vector<Pin*> getInputPins() const { return _inputPins; }
    std::vector<Pin*> getOutputPins() const { return _outputPins; }
    Pin* getPin(std::string pin_name);
    std::vector<Site*> getSites() const { return _sites; }
    std::vector<Bin*> getBins() const { return _bins; }
    double getQDelay();
    LibCell* getLibCell() const { return _lib_cell; }

    void setX(int x);
    void setY(int y);
    void setXY(int x, int y);
    void setInstName(std::string inst_name);
    void addSite(Site* site);
    void addBin(Bin* bin);
    void addPin(Pin* pin);

    void removeSite(Site* site);
    void removeBin(Bin* bin);

    void deletePins();

    bool checkOverlap();

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