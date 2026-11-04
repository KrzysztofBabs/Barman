#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <gui/common/FrontendApplication.hpp>
#include <cstdint>

extern "C" int32_t waga_gramy;

extern "C" {
    #include "main.h" // Żeby mieć dostęp do funkcji STM32
    void SetValveState(uint8_t valve_id, uint8_t state); // Tę funkcję zaraz napiszemy w main.c
}
#include "pour.h"

static FrontendApplication& application()
{
    return *static_cast<FrontendApplication*>(touchgfx::Application::getInstance());
}

void Model::setValve(bool state) {
    SetValveState(1, state ? 1 : 0); // Wysyłamy prośbę do main.c
}

Model::Model() : modelListener(0), activeScreen(SCREEN_1), returnScreen(SCREEN_1), lastPourId(0)
{

}

void Model::leavePourScreen()
{
    if (returnScreen == SCREEN_2)
    {
        application().gotoScreen2ScreenNoTransition();
    }
    else
    {
        application().gotoScreen1ScreenNoTransition();
    }
}

void Model::tick()
{
    static int counter = 0;
    counter++;
    if (counter % 6 == 0) { // ~10x na sekundę (60 FPS / 6)
        modelListener->UpdateWeight(waga_gramy);
    }

    // Nowe nalewanie (z ekranu albo z aplikacji) - pokazujemy PourScreen,
    // niezaleznie od tego, ktory ekran jest otwarty.
    PourState pour;
    Pour_GetState(&pour);
    if (pour.kind != POUR_IDLE && pour.pourId != lastPourId)
    {
        lastPourId = pour.pourId;
        if (activeScreen != SCREEN_POUR)
        {
            returnScreen = activeScreen;
            application().gotoPourScreen();
        }
    }
}
