#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    virtual void SetWeight(int32_t weight);
    virtual void pourButtonClicked(); // Deklaracja funkcji przycisku
    virtual void stopButtonClicked(); // Przycisk STOP - zamyka zawór
    
protected:
};

#endif // SCREEN1VIEW_HPP
