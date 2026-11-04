#include <gui/common/FrontendApplication.hpp>
#include <gui/common/FrontendHeap.hpp>
#include <touchgfx/transitions/NoTransition.hpp>
#include <gui/pourscreen_screen/PourScreenView.hpp>
#include <gui/pourscreen_screen/PourScreenPresenter.hpp>

FrontendApplication::FrontendApplication(Model& m, FrontendHeap& heap)
    : FrontendApplicationBase(m, heap)
{

}

void FrontendApplication::gotoPourScreen()
{
    pourTransitionCallback = touchgfx::Callback<FrontendApplication>(this, &FrontendApplication::gotoPourScreenImpl);
    pendingScreenTransitionCallback = &pourTransitionCallback;
}

void FrontendApplication::gotoPourScreenImpl()
{
    touchgfx::makeTransition<PourScreenView, PourScreenPresenter, touchgfx::NoTransition, Model >(&currentScreen, &currentPresenter, frontendHeap, &currentTransition, &model);
}
