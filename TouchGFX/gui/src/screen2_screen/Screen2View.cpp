#include <gui/screen2_screen/Screen2View.hpp>

Screen2View::Screen2View() : lastBottleNamesVersion(0)
{

}

void Screen2View::setupScreen()
{
    Screen2ViewBase::setupScreen();
    updateBottleNames();
}

void Screen2View::tearDownScreen()
{
    Screen2ViewBase::tearDownScreen();
}

void Screen2View::selectDrink1(){
    Pour_StartBottle(1);
}

void Screen2View::selectDrink2(){
    Pour_StartBottle(2);
}

void Screen2View::selectDrink3(){
    Pour_StartBottle(3);
}

void Screen2View::selectDrink4(){
    Pour_StartBottle(4);
}

void Screen2View::updateBottleNames()
{
    // Wersje zapamietujemy przed kopiowaniem - gdy nazwa zmieni sie w trakcie,
    // w nastepnym ticku odswiezymy jeszcze raz.
    lastBottleNamesVersion = GetBottleNamesVersion();

    Unicode::strncpy(BottleName1Buffer, GetBottleName(1), BOTTLENAME1_SIZE);
    BottleName1Buffer[BOTTLENAME1_SIZE - 1] = 0;
    BottleName1.invalidate();

    Unicode::snprintf(Bottle1AmountBuffer, BOTTLE1AMOUNT_SIZE, "%d", (int)GetBottleVolume(1));
    Bottle1Amount.invalidate();

    Unicode::strncpy(BottleName2Buffer, GetBottleName(2), BOTTLENAME2_SIZE);
    BottleName2Buffer[BOTTLENAME2_SIZE - 1] = 0;
    BottleName2.invalidate();

    Unicode::snprintf(Bottle2AmountBuffer, BOTTLE2AMOUNT_SIZE, "%d", (int)GetBottleVolume(2));
    Bottle2Amount.invalidate();

    Unicode::strncpy(BottleName3Buffer, GetBottleName(3), BOTTLENAME3_SIZE);
    BottleName3Buffer[BOTTLENAME3_SIZE - 1] = 0;
    BottleName3.invalidate();

    Unicode::snprintf(Bottle3AmountBuffer, BOTTLE3AMOUNT_SIZE, "%d", (int)GetBottleVolume(3));
    Bottle3Amount.invalidate();

    Unicode::strncpy(BottleName4Buffer, GetBottleName(4), BOTTLENAME4_SIZE);
    BottleName4Buffer[BOTTLENAME4_SIZE - 1] = 0;
    BottleName4.invalidate();

    Unicode::snprintf(Bottle4AmountBuffer, BOTTLE4AMOUNT_SIZE, "%d", (int)GetBottleVolume(4));
    Bottle4Amount.invalidate();
}

void Screen2View::handleTickEvent(){
    Screen2ViewBase::handleTickEvent();

    if (GetBottleNamesVersion() != lastBottleNamesVersion)
    {
        updateBottleNames();
    }
}
