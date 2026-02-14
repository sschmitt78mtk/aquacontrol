/*
* backlight_fader.hpp
* hilft beim einblenden und ausblenden von Backlights, die über PWM gesteuert werden
* -brightness gibt einen Wert für 8bit pwm counter zurück
* -upd() muss vorher getriggert werden
* -busy zeigt an, ob fading abgeschlossen ist
* -verwendet millis
* erstellt:			23.01.2016
* modifiziert:      18.09.2025 für ES8266 (10 Bit Auflösung)
* Autor:			SC
*/


#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <stdint.h> // für Types 
#include <Arduino.h> // für millis

class backlight
{
    public:

        bool upd (void);        // returns 1 if pwm changed;
        void init (void);
        void fade (uint16_t brightness_target1, uint32_t duration_ms = 2000);
        bool busy;
        uint16_t brightness;     // returns value for 8bit pwm

        backlight();
        ~backlight();
    protected:
    private:

        uint32_t xstarted_ms;
        uint32_t xend_ms;
        uint32_t xduration_ms;
        uint32_t xduration_ms_last;
        uint32_t next_update_ms;
        uint32_t next_update_stepsize;
        uint16_t brightness_last;
        uint16_t brightness_started;
        uint16_t brightness_target;
        uint16_t brightness_target_last;

        uint16_t calc_brightness (uint32_t dxstart, uint32_t dx, uint16_t dystart, int16_t dy);


};

#endif // BACKLIGHT_H

