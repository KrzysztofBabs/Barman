#include <gui/screen2_screen/Screen2View.hpp>



const DrinkRecipe Screen2View::recipeList[3] = {
    // Przepis 0: Opcja A (Butla 1 przez 5s, potem Butla 2 przez 7s)
    {
        "1. Opcja A",
        {
            {1, 300, 50},  // Krok 0: Butelka 1, 5s (300 tików), 50g
            {2, 420, 100}, // Krok 1: Butelka 2, 7s (420 tików), 100g
            {0, 0, 0}      // Koniec
        }
    },
    // Przepis 1: Szybki Mix (Butla 3 przez 3s, potem Butla 1 przez 4s)
    {
        "2. Szybki Mix",
        {
            {3, 180, 30},  // Krok 0: Butelka 3, 3s (180 tików)
            {1, 240, 60},  // Krok 1: Butelka 1, 4s (240 tików)
            {0, 0, 0}
        }
    },
    // Przepis 2: Shot (Butla 4 przez 2s)
    {
        "3. Shot",
        {
            {4, 120, 25},  // Krok 0: Butelka 4, 2s (120 tików)
            {0, 0, 0}
        }
    }
};

Screen2View::Screen2View():currentState(IDLE), tickCounter(0), activeBottle(0), currentRecipeIdx(0), currentStepIdx(0), selectedRecipeIndex(0)
{

}

void Screen2View::setupScreen()
{
    Screen2ViewBase::setupScreen();
}

void Screen2View::tearDownScreen()
{
    Screen2ViewBase::tearDownScreen();
}



void Screen2View::scrollWheel1UpdateItem(CustomContainer1& item, int16_t itemIndex)
{
    if (itemIndex >= 0 && itemIndex < 3)
    {
        item.setRecipeName(recipeList[itemIndex].name);
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
    for (int16_t i = 0; i < 3; i++)
    {
        scrollWheel1.itemChanged(i);
    }
}

// Wywoływane po naciśnięciu przycisku "NALEJ" pod listą
void Screen2View::startSelectedRecipe()
{
    int16_t selected = scrollWheel1.getSelectedItem();
    startRecipe((uint8_t)selected);
}

// Start przepisu wieloetapowego
void Screen2View::startRecipe(uint8_t recipeIndex)
{
    if (currentState == IDLE)
    {
        currentRecipeIdx = recipeIndex;
        currentStepIdx = 0;
        tickCounter = 0;
        currentState = POURING_RECIPE;

        openCurrentStepValve();
    }
}

// Otwieranie zaworu dla bieżącego kroku
void Screen2View::openCurrentStepValve()
{
    uint8_t bottle = recipeList[currentRecipeIdx].steps[currentStepIdx].bottleId;
    if (bottle != 0)
    {
        SetValveState(bottle, 1); // 1 = Otwórz zawór (0V)
    }
}




void Screen2View::selectDrink1(){
    startPouring(1);
}

void Screen2View::selectDrink2(){
    startPouring(2);
}


void Screen2View::selectDrink3(){
    startPouring(3);
}



void Screen2View::selectDrink4(){
    startPouring(4);
}




void Screen2View::startPouring(uint8_t bottleNum){
    if(currentState==IDLE){
        activeBottle=bottleNum;
        currentState=POURING;
        tickCounter=0;
        SetValveState(activeBottle,1);
    }
}


void Screen2View::handleTickEvent(){
    Screen2ViewBase::handleTickEvent();

    // Callbacki AnimateToItem/ItemSelected scroll wheela nie sa podlaczone w Designerze,
    // wiec odpytujemy aktualnie wybrany element i odswiezamy podswietlenie, gdy sie zmieni.
    setSelectedRecipe(scrollWheel1.getSelectedItem());

    if(currentState==POURING){
        tickCounter++;

        if(tickCounter>=180){
            SetValveState(activeBottle,0);
            currentState=IDLE;
            activeBottle=0;
            tickCounter=0;
        }
    }
    else if(currentState==POURING_RECIPE){
        tickCounter++;
        // Tryb przepisu wieloetapowego:
        RecipeStep currentStep = recipeList[currentRecipeIdx].steps[currentStepIdx];

        if (tickCounter >= currentStep.duration)
        {
            // 1. Zamknij bieżący zawór
            SetValveState(currentStep.bottleId, 0);

            // 2. Przejdź do kolejnego kroku
            currentStepIdx++;
            tickCounter = 0;

            // 3. Sprawdź, czy to kolejny etap, czy koniec drinka
            if (recipeList[currentRecipeIdx].steps[currentStepIdx].bottleId != 0)
            {
                openCurrentStepValve(); // Otwórz następną butelkę
            }
            else
            {
                currentState = IDLE; // Koniec całego drinka
                currentRecipeIdx = 0;
                currentStepIdx = 0;
            }
        }
    }
}