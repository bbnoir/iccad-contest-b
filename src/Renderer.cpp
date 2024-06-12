#include "Renderer.h"
#include "Solver.h"
#include "param.h"
#include "Comb.h"
#include "FF.h"
#include "Pin.h"

int WINDOW_X = 100;
int WINDOW_Y = 100;
int WINDOW_WIDTH = 1080;
int WINDOW_HEIGHT = 720;

int MOVE_STEP = 6;

int IO_PIN_SIZE = 2;
int CELL_PIN_SIZE = 2;

Renderer::Renderer(Solver* solver)
{
    _solver = solver;
}

Renderer::~Renderer()
{

}

void Renderer::init()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        exit(1);
    }
    _window = SDL_CreateWindow("Placement", WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (_window == nullptr)
    {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        exit(1);
    }

    _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (_renderer == nullptr)
    {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(_window);
        SDL_Quit();
        exit(1);
    }

    if(TTF_Init() == -1)
    {
        std::cerr << "TTF_Init Error: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(_renderer);
        SDL_DestroyWindow(_window);
        SDL_Quit();
        exit(1);
    }

    // set m_render_viewport
    _render_viewport = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    _scale = 1.0;
    // let the die fit the window
    _die = {10, 10, 1060, 700};
    double die_ratio = (double)DIE_UP_RIGHT_X / DIE_UP_RIGHT_Y;
    double window_ratio = (double)WINDOW_WIDTH / WINDOW_HEIGHT;
    if (die_ratio > window_ratio)
    {
        _die.w = WINDOW_WIDTH - 20;
        _die.h = _die.w / die_ratio;
    }
    else
    {
        _die.h = WINDOW_HEIGHT - 20;
        _die.w = _die.h * die_ratio;
    }
}

void Renderer::render_text(const char* text, int x, int y, int textSize, SDL_Color color)
{
    if (cellOnly)
    {
        return;
    }
    TTF_Font* font = TTF_OpenFont("tool/font/Roboto-Regular.ttf", textSize);
    if (font == nullptr)
    {
        std::cerr << "TTF_OpenFont Error: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(_renderer);
        SDL_DestroyWindow(_window);
        TTF_Quit();
        SDL_Quit();
        exit(1);
    }

    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(_renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(_renderer, texture, nullptr, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    TTF_CloseFont(font);
}

void Renderer::render()
{
    init();
    // calculate the transform between actual coordinate and window coordinate
    double x_scale = (double)_die.w / DIE_UP_RIGHT_X;
    double y_scale = (double)_die.h / DIE_UP_RIGHT_Y;

    bool quit = false;
    while(!quit)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
                break;
            }
        }
        // handle key events
        unsigned char const *keys = SDL_GetKeyboardState(nullptr);
        if(keys[SDL_SCANCODE_Q])
        {
            _scale += 0.05;
            if (_scale > 3.0)
            {
                _scale = 3.0;
            }
        } else if(keys[SDL_SCANCODE_E])
        {
            _scale -= 0.05;
            if (_scale < 0.5)
            {
                _scale = 0.5;
            }
        }
        else if(keys[SDL_SCANCODE_W])
        {
            _render_viewport.y += MOVE_STEP;
        } else if(keys[SDL_SCANCODE_S])
        {
            _render_viewport.y -= MOVE_STEP;
        } else if(keys[SDL_SCANCODE_A])
        {
            _render_viewport.x += MOVE_STEP;
        } else if(keys[SDL_SCANCODE_D])
        {
            _render_viewport.x -= MOVE_STEP;
        }

        SDL_SetRenderDrawColor(_renderer, 255, 255, 255, 255);
        SDL_RenderClear(_renderer);
        // draw Die border
        SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(_renderer, &_die);
        // draw grid of bins
        for (int i = 0; i < DIE_UP_RIGHT_X; i += BIN_WIDTH)
        {
            SDL_RenderDrawLine(_renderer, 10 + i * x_scale, 10, 10 + i * x_scale, 10 + _die.h);
        }
        for (int i = 0; i < DIE_UP_RIGHT_Y; i += BIN_HEIGHT)
        {
            SDL_RenderDrawLine(_renderer, 10, 10 + i * y_scale, 10 + _die.w, 10 + i * y_scale);
        }
        // draw placement row
        for(auto row : _solver->_placementRows)
        {
            int rect_x = static_cast<int>(10 + row.startX * x_scale);
            int rect_y = static_cast<int>(10 + row.startY * y_scale);
            int rect_width = static_cast<int>(row.siteWidth * row.numSites * x_scale);
            int rect_height = static_cast<int>(row.siteHeight * y_scale);
            SDL_Rect rect = {rect_x, rect_y, rect_width, rect_height};
            SDL_SetRenderDrawColor(_renderer, 173, 216, 230, 255); // Light blue color
            SDL_RenderFillRect(_renderer, &rect); // Fill the rectangle with the specified color
        }
        // draw placement        
        for (auto& comb : _solver->_combs)
        {
            int rect_x = static_cast<int>(10 + comb->getX() * x_scale);
            int rect_y = static_cast<int>(10 + comb->getY() * y_scale);
            int rect_width = static_cast<int>(comb->getWidth() * x_scale);
            int rect_height = static_cast<int>(comb->getHeight() * y_scale);
            rect_width = std::max(rect_width, 1);
            rect_height = std::max(rect_height, 1);
            SDL_Rect rect = {rect_x, rect_y, rect_width, rect_height};
            SDL_SetRenderDrawColor(_renderer, 0, 0, 255, 255);
            SDL_RenderFillRect(_renderer, &rect);           
            render_text(comb->getInstName().c_str(), rect_x, rect_y+rect_height, 16, {0, 0, 0, 255}); 
        }
        for (auto& ff : _solver->_ffs)
        {
            int rect_x = static_cast<int>(10 + ff->getX() * x_scale);
            int rect_y = static_cast<int>(10 + ff->getY() * y_scale);
            int rect_width = static_cast<int>(ff->getWidth() * x_scale);
            int rect_height = static_cast<int>(ff->getHeight() * y_scale);
            rect_width = std::max(rect_width, 1);
            rect_height = std::max(rect_height, 1);
            SDL_Rect rect = {rect_x, rect_y, rect_width, rect_height};
            SDL_SetRenderDrawColor(_renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(_renderer, &rect);
            render_text(ff->getInstName().c_str(), rect_x, rect_y+rect_height, 16, {0, 0, 0, 255});
        }
        // draw input pins
        for (auto& pin : _solver->_inputPins)
        {
            int rect_x = static_cast<int>(10 + pin->getX() * x_scale - IO_PIN_SIZE/2);
            int rect_y = static_cast<int>(10 + pin->getY() * y_scale - IO_PIN_SIZE/2);
            SDL_Rect rect = {rect_x, rect_y, IO_PIN_SIZE, IO_PIN_SIZE};
            SDL_SetRenderDrawColor(_renderer, 125, 0, 0, 255);
            SDL_RenderFillRect(_renderer, &rect);
            render_text(pin->getName().c_str(), rect_x, rect_y+IO_PIN_SIZE, 16, {0, 0, 0, 255});
        }
        // draw output pins
        for (auto& pin : _solver->_outputPins)
        {
            int rect_x = static_cast<int>(10 + pin->getX() * x_scale - IO_PIN_SIZE/2);
            int rect_y = static_cast<int>(10 + pin->getY() * y_scale - IO_PIN_SIZE/2);
            SDL_Rect rect = {rect_x, rect_y, IO_PIN_SIZE, IO_PIN_SIZE};
            SDL_SetRenderDrawColor(_renderer, 0, 0, 125, 255);
            SDL_RenderFillRect(_renderer, &rect);   
            render_text(pin->getName().c_str(), rect_x, rect_y+IO_PIN_SIZE, 16, {0, 0, 0, 255});
        }
        if (!cellOnly)
        {
            // draw cell pins
            for (auto& cell : _solver->_ffs)
            {
                for (auto& pin : cell->getPins())
                {
                    int rect_x = static_cast<int>(10 + pin->getGlobalX() * x_scale - CELL_PIN_SIZE/2.0);
                    int rect_y = static_cast<int>(10 + pin->getGlobalY() * y_scale - CELL_PIN_SIZE/2.0);
                    SDL_Rect rect = {rect_x, rect_y, CELL_PIN_SIZE, CELL_PIN_SIZE};
                    SDL_SetRenderDrawColor(_renderer, 0, 255, 0, 255);
                    SDL_RenderFillRect(_renderer, &rect);
                    render_text(pin->getName().c_str(), rect_x, rect_y+CELL_PIN_SIZE, 8, {0, 0, 0, 255});
                }
            }
            // draw net
            // draw straight line
            int r, g, b;
            r = g = b = 0;
            for (auto& net : _solver->_nets)
            {   
                r += 20;
                g += 30;
                b += 40;
                r %= 256;
                g %= 256;
                b %= 256;
                SDL_SetRenderDrawColor(_renderer, r, g, b, 255);
                std::vector<Pin*> pins = net->getPins();
                for (long unsigned int i = 0; i < pins.size() - 1; i++)
                {
                    int x1 = static_cast<int>(10 + pins[i]->getGlobalX() * x_scale);
                    int y1 = static_cast<int>(10 + pins[i]->getGlobalY() * y_scale);
                    int x2 = static_cast<int>(10 + pins[i+1]->getGlobalX() * x_scale);
                    int y2 = static_cast<int>(10 + pins[i+1]->getGlobalY() * y_scale);
                    SDL_SetRenderDrawColor(_renderer, r, g, b, 255);
                    SDL_RenderDrawLine(_renderer, x1, y1, x2, y2);
                }
            }
        }
        // update the window
        SDL_RenderSetScale(_renderer, _scale, _scale);
        SDL_RenderSetViewport(_renderer, &_render_viewport);
        SDL_RenderPresent(_renderer);
    }
    // save the die placement as a png
    SDL_Surface* surface = SDL_CreateRGBSurface(0, _die.w, _die.h, 32, 0, 0, 0, 0);
    SDL_RenderReadPixels(_renderer, &_die, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch);
    SDL_SaveBMP(surface, "out/placement.bmp");
    SDL_FreeSurface(surface);
    close();
}

void Renderer::close()
{
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_window);
    TTF_Quit();
    SDL_Quit();
    std::cout << "Renderer closed" << std::endl;
}