#ifndef POURSCREENPRESENTER_HPP
#define POURSCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class PourScreenView;

class PourScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    PourScreenPresenter(PourScreenView& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();

    virtual ~PourScreenPresenter() {}

    // Widok skonczyl pokazywac "Gotowe!" - wracamy na poprzedni ekran.
    void pourScreenDone();

    virtual void UpdateWeight(int32_t weight);

private:
    PourScreenPresenter();

    PourScreenView& view;
};

#endif // POURSCREENPRESENTER_HPP
