#include <gui/screen2_screen/Screen2View.hpp>

Screen2View::Screen2View() : selectedRecipeIndex(0), lastBottleNamesVersion(0)
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



void Screen2View::scrollWheel1UpdateItem(CustomContainer1& item, int16_t itemIndex)
{
    if (itemIndex >= 0 && itemIndex < POUR_RECIPE_COUNT)
    {
        item.setRecipeName(Pour_GetRecipeName((uint8_t)itemIndex));
        item.setSelected(itemIndex == selectedRecipeIndex);
    }
}

void Screen2View::setSelectedRecipe(int16_t itemIndex)
{
    if (itemIndex == selectedRecipeIndex)
    {
        return;
    }

    selectedRecipeIndex = itemIndex;

    // itemChanged() wymusza ponowne wywolanie scrollWheel1UpdateItem() dla kazdego
    // elementu, dzieki czemu kolory podswietlenia sa faktycznie przeliczane.
    // Samo invalidate() tylko przerysowuje elementy w ich obecnym stanie.
    for (int16_t i = 0; i < POUR_RECIPE_COUNT; i++)
    {
        scrollWheel1.itemChanged(i);
    }
}

// Wywoływane po naciśnięciu przycisku "NALEJ" pod listą
void Screen2View::startSelectedRecipe()
{
    int16_t selected = scrollWheel1.getSelectedItem();
    if (selected >= 0)
    {
        Pour_StartRecipe((uint8_t)selected);
    }
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

    Unicode::strncpy(BottleName3Buffer, GetBottleName(3), BOTTLENAME3_SIZE);
    BottleName3Buffer[BOTTLENAME3_SIZE - 1] = 0;
    BottleName3.invalidate();

    Unicode::strncpy(BottleName4Buffer, GetBottleName(4), BOTTLENAME4_SIZE);
    BottleName4Buffer[BOTTLENAME4_SIZE - 1] = 0;
    BottleName4.invalidate();
}

void Screen2View::handleTickEvent(){
    Screen2ViewBase::handleTickEvent();

    if (GetBottleNamesVersion() != lastBottleNamesVersion)
    {
        updateBottleNames();
    }

    // Callbacki AnimateToItem/ItemSelected scroll wheela nie sa podlaczone w Designerze,
    // wiec odpytujemy aktualnie wybrany element i odswiezamy podswietlenie, gdy sie zmieni.
    setSelectedRecipe(scrollWheel1.getSelectedItem());
}
