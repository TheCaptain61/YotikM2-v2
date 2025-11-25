#include "Profiles.h"

void applyCropProfile(uint8_t id, SystemSettings &s) {
    s.cropProfile = id;

    switch (id) {
        case 1: // 🍅 Помидоры
            s.comfortTempMin       = 20.0f;
            s.comfortTempMax       = 28.0f;
            s.comfortHumMin        = 40.0f;
            s.comfortHumMax        = 65.0f;
            s.soilMoistureSetpoint = 55.0f;
            s.lightLuxMin          = 6000.0f;
            s.lightMode            = 2;
            break;

        case 2: // 🥒 Огурцы
            s.comfortTempMin       = 22.0f;
            s.comfortTempMax       = 30.0f;
            s.comfortHumMin        = 60.0f;
            s.comfortHumMax        = 80.0f;
            s.soilMoistureSetpoint = 70.0f;
            s.lightLuxMin          = 5000.0f;
            s.lightMode            = 1;
            break;

        case 3: // 🌿 Зелень
            s.comfortTempMin       = 18.0f;
            s.comfortTempMax       = 24.0f;
            s.comfortHumMin        = 50.0f;
            s.comfortHumMax        = 70.0f;
            s.soilMoistureSetpoint = 60.0f;
            s.lightLuxMin          = 4000.0f;
            s.lightMode            = 1;
            break;

        case 4: // 🌿 Гибискус
            s.comfortTempMin       = 20.0f;
            s.comfortTempMax       = 26.0f;
            s.comfortHumMin        = 45.0f;
            s.comfortHumMax        = 65.0f;
            s.soilMoistureSetpoint = 60.0f;
            s.lightLuxMin          = 200.0f;
            s.lightMode            = 1;
            break;

        default: // 0 = custom
            // значения оставляем как есть
            break;
    }
}