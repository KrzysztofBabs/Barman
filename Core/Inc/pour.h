#ifndef POUR_H
#define POUR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Nalewanie - jedyne miejsce, ktore otwiera zawory. Dziala niezaleznie od
// otwartego ekranu. Start/Stop mozna wolac z ekranu i z przerwania UART.

#define POUR_STEP_MAX      4   // maks. krokow w jednym drinku
#define POUR_RECIPE_COUNT  3

typedef enum
{
    POUR_IDLE = 0,   // nic nie leci
    POUR_BOTTLE,     // jedna butelka (przycisk / "#1".."#4")
    POUR_RECIPE,     // przepis z listy (NALEJ / "#A".."#C")
    POUR_MIX         // wlasny drink z aplikacji ("$MIX")
} PourKind;

typedef struct
{
    PourKind kind;
    uint8_t recipe;     // przy POUR_RECIPE: 0..POUR_RECIPE_COUNT-1
    uint8_t bottle;     // butelka z biezacego kroku (1..4)
    uint8_t step;       // biezacy krok, od 0
    uint8_t stepCount;
    uint8_t stopped;    // 1 = ostatnie nalewanie przerwal STOP
    uint16_t pourId;    // zwiekszany przy kazdym starcie - po nim GUI poznaje nowe nalewanie
    uint32_t elapsedMs; // od startu calego drinka
    uint32_t totalMs;   // laczny czas drinka
} PourState;

// Tworzy zadanie nalewania. Wolac przed osKernelStart().
void Pour_Init(void);

// Zwracaja 1 = start, 0 = odrzucone (cos juz leci albo zle dane).
uint8_t Pour_StartBottle(uint8_t bottle);
uint8_t Pour_StartRecipe(uint8_t recipe);
// times w 0,1 s, kroki nalewane po kolei.
uint8_t Pour_StartMix(const uint8_t *bottles, const uint16_t *times, uint8_t count);

// Zamyka wszystkie zawory od razu.
void Pour_Stop(void);

uint8_t Pour_IsBusy(void);
void Pour_GetState(PourState *state);
const char *Pour_GetRecipeName(uint8_t recipe);

#ifdef __cplusplus
}
#endif

#endif // POUR_H
