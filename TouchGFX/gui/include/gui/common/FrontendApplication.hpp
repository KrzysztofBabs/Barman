#ifndef FRONTENDAPPLICATION_HPP
#define FRONTENDAPPLICATION_HPP

#include <gui_generated/common/FrontendApplicationBase.hpp>

class FrontendHeap;

using namespace touchgfx;

class FrontendApplication : public FrontendApplicationBase
{
public:
    FrontendApplication(Model& m, FrontendHeap& heap);
    virtual ~FrontendApplication() { }

    virtual void handleTickEvent()
    {
        model.tick();
        FrontendApplicationBase::handleTickEvent();
    }

    // Designer nie generuje przejscia na PourScreen (zadna interakcja do niego
    // nie prowadzi) - wlacza go Model, gdy zaczyna sie nalewanie.
    void gotoPourScreen();

private:
    void gotoPourScreenImpl();

    touchgfx::Callback<FrontendApplication> pourTransitionCallback;
};

#endif // FRONTENDAPPLICATION_HPP
