#include "Solver.h"

Solver::Solver()
{
}

Solver::~Solver()
{
}

void Solver::parse_input(std::string filename)
{
    using namespace std;
    ifstream in(filename);
    if(!in.good())
    {
        cout << "Error opening file: " << filename << endl;
        return;
    }

    string line;
    // Read the cost metrics
    
    // Read the die info

    // Read I/O info

    // Read cell library

    // Read instance info

    // Read net info

    // Read bin info

    // Read placement rows
    
    // Read timing info

    in.close();
}

void Solver::solve()
{
}