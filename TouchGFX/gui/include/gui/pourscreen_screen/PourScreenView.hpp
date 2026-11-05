#ifndef POURSCREENVIEW_HPP
#define POURSCREENVIEW_HPP

#include <gui_generated/pourscreen_screen/PourScreenViewBase.hpp>
#include <gui/pourscreen_screen/PourScreenPresenter.hpp>
#include "pour.h"

// Pokazuje, co leci teraz (stan z modulu pour), a po koncu przez chwile
// "Gotowe!" / "Zatrzymano" i wraca na poprzedni ekran.
class PourScreenView : public PourScreenViewBase
{
public:
    PourScreenView();
    virtual ~PourScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    virtual void stopPouring();
    void setWeight(int32_t grams);
protected:
    void showPouring(const PourState& state);
    void showFinished(bool stopped);

    bool pouring;
    uint16_t finishTicks;   // > 0 = ile tickow zostalo do powrotu na poprzedni ekran
};

#endif // POURSCREENVIEW_HPP
