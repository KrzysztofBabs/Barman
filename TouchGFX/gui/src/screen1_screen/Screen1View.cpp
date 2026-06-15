#include <gui/screen1_screen/Screen1View.hpp>

Screen1View::Screen1View()
{

}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}



void Screen1View::SetWeight(int32_t weight) {
    //    %d zamiast %ld.
    Unicode::snprintf(textArea1Buffer, TEXTAREA1_SIZE, "%d", (int)weight);
    textArea1.invalidate();
}

void Screen1View::pourButtonClicked() {
    // Prosimy Presentera, żeby przekazał prośbę o lanie do Modelu
    presenter->startPouring();
}

void Screen1View::stopButtonClicked() {
    // Prosimy Presentera, żeby przekazał prośbę o zatrzymaniu lania
    presenter->stopPouring();
}