#pragma once
#include <iostream>
#include <algorithm>
#include "SDL.h"
#include "SDL_ttf.h"
#include "param.h"

class Solver;

class Renderer
{
    private:
        Solver* _solver;

        SDL_Window* _window;
        SDL_Renderer* _renderer;
        SDL_Rect _render_viewport;
        SDL_Rect _die;
        double _scale;

        void init();
        void close();

        void render_text(const char* text, int x, int y, int textSize, SDL_Color color);
    public:
        Renderer(Solver* solver);
        ~Renderer();

        void render();
};