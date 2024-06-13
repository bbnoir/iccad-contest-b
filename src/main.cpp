#include <iostream>
#include <string>
#include "Solver.h"
#include "Renderer.h"

int main(int argc, char* argv[])
{
    // format ./$binary_name <input.txt> <output.txt>
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input.txt> <output.txt>" << std::endl;
        return 1;
    }
    std::string input_file = argv[1];
    std::string output_file = argv[2];
    Solver* solver = new Solver();

    solver->parse_input(input_file);
    solver->solve();
    solver->dump(output_file);

    Renderer* renderer = new Renderer(solver);
    // renderer->render();

    // solver->display();

    delete renderer;
    delete solver;
    return 0;
}