#pragma once
#include <string>
#include "Site.h"
#include "Pin.h"

class Site;
class Pin;

// Pin type
enum class CellType
{
    LIB,
    INST
};

class Cell
{
public:
    Cell();
    // constructor for library cells
    Cell(int width, int height, std::string cell_name);
    // constructor for instance cells
    Cell(int x, int y, int width, int height, std::string inst_name,std::string cell_name);
    ~Cell();

    int getX();
    int getY();
    int getWidth();
    int getHeight();
    int getArea();
    std::string getInstName();
    std::string getCellName();
    std::vector<Pin*> getPins();
    Pin* getPin(std::string pin_name);

    void setX(int x);
    void setY(int y);
    void setCellName(std::string cell_name);
    void setInstName(std::string inst_name);
    void setSite(Site* site);

    void addPin(Pin* pin);

protected:
    CellType _type;
    int _x;
    int _y;
    int _width;
    int _height;
    int _area;
    std::string _inst_name;
    std::string _cell_name;
    std::vector<Pin*> pins;
    // Site where the cell is placed
    Site* _site;
};