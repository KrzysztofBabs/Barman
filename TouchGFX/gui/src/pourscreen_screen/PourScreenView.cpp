#include <gui/pourscreen_screen/PourScreenView.hpp>

extern "C" const char *GetBottleName(uint8_t bottle_id);

// Ile "Gotowe!" wisi na ekranie przed powrotem (60 tickow = 1 s).
static const uint16_t FINISH_TICKS = 120;

PourScreenView::PourScreenView() : pouring(false), finishTicks(0)
{

}

void PourScreenView::setupScreen()
{
    PourScreenViewBase::setupScreen();

    // Gdy nalewanie skonczylo sie juz w trakcie przejscia na ten ekran,
    // pierwszy tick od razu pokaze "Gotowe!".
    pouring = true;
    finishTicks = 0;
    handleTickEvent();
}

void PourScreenView::tearDownScreen()
{
    PourScreenViewBase::tearDownScreen();
}

void PourScreenView::stopPouring()
{
    Pour_Stop();
}

void PourScreenView::handleTickEvent()
{
    PourScreenViewBase::handleTickEvent();

    PourState state;
    Pour_GetState(&state);

    if (state.kind != POUR_IDLE)
    {
        // Nowe nalewanie w trakcie "Gotowe!" - zostajemy na ekranie.
        pouring = true;
        finishTicks = 0;
        showPouring(state);
    }
    else if (pouring)
    {
        pouring = false;
        showFinished(state.stopped != 0);
        finishTicks = FINISH_TICKS;
    }
    else if (finishTicks > 0)
    {
        finishTicks--;
        if (finishTicks == 0)
        {
            presenter->pourScreenDone();
        }
    }
}

void PourScreenView::showPouring(const PourState& state)
{
    switch (state.kind)
    {
        case POUR_BOTTLE:
            Unicode::snprintf(textArea1Buffer, TEXTAREA1_SIZE, "Butelka %d", (int)state.bottle);
            break;
        case POUR_RECIPE:
            Unicode::strncpy(textArea1Buffer, Pour_GetRecipeName(state.recipe), TEXTAREA1_SIZE);
            break;
        default:
            Unicode::fromUTF8((const uint8_t*)"Własny drink", textArea1Buffer, TEXTAREA1_SIZE);
            break;
    }
    textArea1Buffer[TEXTAREA1_SIZE - 1] = 0;
    textArea1.invalidate();

    Unicode::UnicodeChar name[17];
    Unicode::strncpy(name, GetBottleName(state.bottle), 17);
    name[16] = 0;
    Unicode::snprintf(bottleTextBuffer, BOTTLETEXT_SIZE, "Butelka %d: %s", (int)state.bottle, name);
    bottleText.invalidate();

    Unicode::snprintf(stepTextBuffer, STEPTEXT_SIZE, "Krok %d/%d", (int)state.step + 1, (int)state.stepCount);
    stepText.invalidate();

    int percent = (state.totalMs > 0) ? (int)(state.elapsedMs * 100u / state.totalMs) : 0;
    progressBar.setValue(percent);

    // Pozostaly czas w dziesiatych sekundy, zaokraglony w gore.
    uint32_t tenths = (state.totalMs - state.elapsedMs + 99u) / 100u;
    Unicode::snprintf(timeTextBuffer, TIMETEXT_SIZE, "%d,%d s", (int)(tenths / 10u), (int)(tenths % 10u));
    timeText.invalidate();
}

void PourScreenView::showFinished(bool stopped)
{
    Unicode::fromUTF8(stopped ? (const uint8_t*)"Zatrzymano" : (const uint8_t*)"Gotowe!",
                      textArea1Buffer, TEXTAREA1_SIZE);
    textArea1.invalidate();

    bottleTextBuffer[0] = 0;
    bottleText.invalidate();
    stepTextBuffer[0] = 0;
    stepText.invalidate();
    timeTextBuffer[0] = 0;
    timeText.invalidate();

    if (!stopped)
    {
        progressBar.setValue(100);
    }
}
