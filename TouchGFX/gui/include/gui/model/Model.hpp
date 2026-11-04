#ifndef MODEL_HPP
#define MODEL_HPP

#include <cstdint>

class ModelListener;

class Model
{
public:
    enum ScreenId
    {
        SCREEN_1,
        SCREEN_2,
        SCREEN_POUR
    };

    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }
    void setValve(bool state);

    // Wolane z activate() presenterow - Model musi wiedziec, dokad wrocic po nalewaniu.
    void setActiveScreen(ScreenId screen)
    {
        activeScreen = screen;
    }
    // Koniec nalewania - powrot na ekran, z ktorego weszlismy na PourScreen.
    void leavePourScreen();

    void tick();
protected:
    ModelListener* modelListener;
    ScreenId activeScreen;
    ScreenId returnScreen;
    uint16_t lastPourId;
};

#endif // MODEL_HPP
