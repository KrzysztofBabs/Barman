#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <cstdint>

extern "C" int32_t waga_gramy;

extern "C" {
    #include "main.h" // Żeby mieć dostęp do funkcji STM32
    void SetValveState(uint8_t valve_id, uint8_t state); // Tę funkcję zaraz napiszemy w main.c
}

void Model::setValve(bool state) {
    SetValveState(1, state ? 1 : 0); // Wysyłamy prośbę do main.c
}

Model::Model() : modelListener(0)
{

}

void Model::tick()
{
    static int counter = 0;
    counter++;
    if (counter % 6 == 0) { // ~10x na sekundę (60 FPS / 6)
        modelListener->UpdateWeight(waga_gramy);
    }

}
