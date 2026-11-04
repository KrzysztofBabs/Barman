#include "pour.h"
#include "main.h"
#include "cmsis_os.h"
#include <string.h>

#define POUR_BOTTLE_MS  3000u  // przycisk pojedynczej butelki
#define POUR_PERIOD_MS  10u    // co ile zadanie sprawdza czas kroku
#define VALVE_COUNT     4

typedef struct
{
    uint8_t bottle;   // 1..4
    uint16_t ms;
} PourStep;

typedef struct
{
    const char *name;
    uint8_t count;
    PourStep steps[POUR_STEP_MAX];
} PourRecipe;

static const PourRecipe recipes[POUR_RECIPE_COUNT] = {
    { "1. Opcja A",    2, { {1, 5000}, {2, 7000} } },
    { "2. Szybki Mix", 2, { {3, 3000}, {1, 4000} } },
    { "3. Shot",       1, { {4, 2000} } },
};

// Zmieniane tylko z wylaczonymi przerwaniami - Start/Stop przychodza tez z przerwania UART.
static PourStep steps[POUR_STEP_MAX];
static uint8_t stepCount = 0;
static uint8_t stepIdx = 0;
static uint8_t recipeIdx = 0;
static volatile PourKind kind = POUR_IDLE;
// 0 = przyjete, ale zawor pierwszego kroku otworzy dopiero zadanie.
static uint8_t started = 0;
static uint32_t stepStartTick = 0;
static uint32_t doneMs = 0;     // suma czasow zakonczonych krokow
static uint32_t totalMs = 0;
static uint16_t pourId = 0;
static uint8_t stopped = 0;

static osThreadId_t pourTaskHandle;
static const osThreadAttr_t pourTask_attributes = {
    .name = "pourTask",
    .stack_size = 256 * 4,
    .priority = (osPriority_t) osPriorityAboveNormal,
};

static uint8_t Begin(PourKind newKind, uint8_t recipe, const PourStep *src, uint8_t count)
{
    if (count == 0 || count > POUR_STEP_MAX)
    {
        return 0;
    }
    for (uint8_t i = 0; i < count; i++)
    {
        if (src[i].bottle < 1 || src[i].bottle > VALVE_COUNT || src[i].ms == 0)
        {
            return 0;
        }
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (kind != POUR_IDLE)
    {
        __set_PRIMASK(primask);
        return 0;
    }
    memcpy(steps, src, count * sizeof(PourStep));
    stepCount = count;
    stepIdx = 0;
    recipeIdx = recipe;
    started = 0;
    doneMs = 0;
    totalMs = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        totalMs += src[i].ms;
    }
    pourId++;
    stopped = 0;
    kind = newKind;
    __set_PRIMASK(primask);
    return 1;
}

// Czas liczony z HAL_GetTick (1 ms), wiec nie zalezy od tego, jak szybko rysuje sie ekran.
static void PourTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        if (kind != POUR_IDLE)
        {
            uint32_t now = HAL_GetTick();
            if (!started)
            {
                SetValveState(steps[0].bottle, 1);
                stepStartTick = now;
                started = 1;
            }
            else if (now - stepStartTick >= steps[stepIdx].ms)
            {
                SetValveState(steps[stepIdx].bottle, 0);
                doneMs += steps[stepIdx].ms;
                stepIdx++;
                if (stepIdx < stepCount)
                {
                    SetValveState(steps[stepIdx].bottle, 1);
                    stepStartTick = now;
                }
                else
                {
                    kind = POUR_IDLE;
                }
            }
        }
        __set_PRIMASK(primask);
        osDelay(POUR_PERIOD_MS);
    }
}

void Pour_Init(void)
{
    pourTaskHandle = osThreadNew(PourTask, NULL, &pourTask_attributes);
}

uint8_t Pour_StartBottle(uint8_t bottle)
{
    PourStep step = { bottle, POUR_BOTTLE_MS };
    return Begin(POUR_BOTTLE, 0, &step, 1);
}

uint8_t Pour_StartRecipe(uint8_t recipe)
{
    if (recipe >= POUR_RECIPE_COUNT)
    {
        return 0;
    }
    return Begin(POUR_RECIPE, recipe, recipes[recipe].steps, recipes[recipe].count);
}

uint8_t Pour_StartMix(const uint8_t *bottles, const uint16_t *times, uint8_t count)
{
    PourStep mix[POUR_STEP_MAX];
    if (count == 0 || count > POUR_STEP_MAX)
    {
        return 0;
    }
    for (uint8_t i = 0; i < count; i++)
    {
        mix[i].bottle = bottles[i];
        mix[i].ms = (uint16_t)(times[i] * 100u);
    }
    return Begin(POUR_MIX, 0, mix, count);
}

void Pour_Stop(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    for (uint8_t valve = 1; valve <= VALVE_COUNT; valve++)
    {
        SetValveState(valve, 0);
    }
    if (kind != POUR_IDLE)
    {
        stopped = 1;
    }
    kind = POUR_IDLE;
    __set_PRIMASK(primask);
}

uint8_t Pour_IsBusy(void)
{
    return kind != POUR_IDLE;
}

void Pour_GetState(PourState *state)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    state->kind = kind;
    state->recipe = recipeIdx;
    state->bottle = (kind != POUR_IDLE) ? steps[stepIdx].bottle : 0;
    state->step = stepIdx;
    state->stepCount = (kind != POUR_IDLE) ? stepCount : 0;
    state->stopped = stopped;
    state->pourId = pourId;
    state->totalMs = totalMs;
    uint32_t elapsed = doneMs;
    if (kind != POUR_IDLE && started)
    {
        elapsed += HAL_GetTick() - stepStartTick;
    }
    state->elapsedMs = (elapsed < totalMs) ? elapsed : totalMs;
    __set_PRIMASK(primask);
}

const char *Pour_GetRecipeName(uint8_t recipe)
{
    if (recipe >= POUR_RECIPE_COUNT)
    {
        return "";
    }
    return recipes[recipe].name;
}
