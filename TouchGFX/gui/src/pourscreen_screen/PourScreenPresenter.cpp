#include <gui/pourscreen_screen/PourScreenView.hpp>
#include <gui/pourscreen_screen/PourScreenPresenter.hpp>

PourScreenPresenter::PourScreenPresenter(PourScreenView& v)
    : view(v)
{

}

void PourScreenPresenter::activate()
{
    model->setActiveScreen(Model::SCREEN_POUR);
}

void PourScreenPresenter::deactivate()
{

}

void PourScreenPresenter::pourScreenDone()
{
    model->leavePourScreen();
}
