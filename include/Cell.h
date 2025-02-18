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
    ~LibCell();
};

class Cell
{
public:
    Cell();
    // constructor for instance cells
    Cell(int x, int y, std::string inst_name, LibCell* lib_cell);
    ~Cell();

    inline int getX() const { return _x; }
    inline int getY() const { return _y; }
    inline int getWidth() const { return _lib_cell->width; }
    inline int getHeight() const { return _lib_cell->height; }
    inline int getArea() const { return _lib_cell->width * _lib_cell->height; }
    inline std::string getInstName() const { return _inst_name; }
    inline std::string getCellName() const { return _lib_cell->cell_name; }
    inline CellType getCellType() const { return _lib_cell->type; }
    inline std::vector<Pin*> getPins() const { return _pins; }
    inline std::vector<Pin*> getInputPins() const { return _inputPins; }
    inline std::vector<Pin*> getOutputPins() const { return _outputPins; }
    inline double getQDelay() const { return _lib_cell->qDelay; }
    inline LibCell* getLibCell() const { return _lib_cell; }
    Pin* getPin(std::string pin_name);

    void setXY(int x, int y);
    void setInstName(std::string inst_name);
    void addPin(Pin* pin);

    void deletePins();

protected:
    LibCell* _lib_cell;
    int _x;
    int _y;
    std::string _inst_name;
    std::vector<Pin*> _pins;
    std::vector<Pin*> _inputPins;
    std::vector<Pin*> _outputPins;
    Pin* _clkPin;
};