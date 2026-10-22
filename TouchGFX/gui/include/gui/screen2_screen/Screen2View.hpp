#ifndef SCREEN2VIEW_HPP
#define SCREEN2VIEW_HPP

#include <gui_generated/screen2_screen/Screen2ViewBase.hpp>
#include <gui/screen2_screen/Screen2Presenter.hpp>
#include <gui/containers/CustomContainer1.hpp>

extern "C"{
    void SetValveState(uint8_t valve_id, uint8_t state);
    uint8_t BLE_GetCommand(void);
}
struct RecipeStep {
    uint8_t bottleId;  
    uint16_t duration;  
    uint16_t targetGram;  //zeby kiedys wage dolaczyc
};

struct DrinkRecipe {
    const char* name;
    RecipeStep steps[4];
};

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

    virtual void scrollWheel1UpdateItem(CustomContainer1& item, int16_t itemIndex);
    virtual void startSelectedRecipe();

protected:

    int16_t selectedRecipeIndex;
    void setSelectedRecipe(int16_t itemIndex);
    void startPouring(uint8_t bottleNum);
    void startRecipe(uint8_t recipeIndex);
    void openCurrentStepValve();
    void handleBleCommand(uint8_t cmd);
    void stopAll();

    enum MenuState{
        IDLE,
        POURING,
        POURING_RECIPE
    };

    MenuState currentState;
    uint16_t tickCounter;
    uint8_t activeBottle;

    uint8_t currentRecipeIdx;                
    uint8_t currentStepIdx;                   

    static const DrinkRecipe recipeList[3];

};

#endif // SCREEN2VIEW_HPP
