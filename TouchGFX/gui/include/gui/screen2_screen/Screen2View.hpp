#ifndef SCREEN2VIEW_HPP
#define SCREEN2VIEW_HPP

#include <gui_generated/screen2_screen/Screen2ViewBase.hpp>
#include <gui/screen2_screen/Screen2Presenter.hpp>
#include "pour.h"

extern "C"{
    const char *GetBottleName(uint8_t bottle_id);
    uint16_t GetBottleVolume(uint8_t bottle_id);
    uint8_t GetBottleNamesVersion(void);
}

// Ekran tylko zleca nalewanie modulowi pour (Core/Src/pour.c) - czas i zawory
// sa tam, wiec nalewanie nie przerywa sie po zmianie ekranu.
class Screen2View : public Screen2ViewBase
{
public:
    Screen2View();
    virtual ~Screen2View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    virtual void selectDrink1();
    virtual void selectDrink2();
    virtual void selectDrink3();
    virtual void selectDrink4();

    virtual void handleTickEvent();

protected:

    uint8_t lastBottleNamesVersion;

    void updateBottleNames();
};

#endif // SCREEN2VIEW_HPP
