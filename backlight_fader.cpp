/*
* backlight_fader.cpp
* hilft beim einblenden und ausblenden von Backlights, die über PWM gesteuert werden
* -brightness gibt einen Wert für 8bit pwm counter zurück
* -upd() muss vorher getriggert werden
* -verwendet millis
* erstellt:			23.01.2016
* modifiziert:      01.06.2025 für ES8266 (10 Bit Auflösung)
*                   26.11.1025 Anpassung für lange Fadingzeiten > 34 Minuten
* Autor:			SC
*/

#include "backlight_fader.hpp"

backlight::backlight()
{
}
backlight::~backlight()
{
}


void backlight::init (void){
    next_update_ms=0;
    next_update_stepsize = 100;
    brightness = 0;
    brightness_last = 0;
    busy = 0;
    xend_ms = 0xffffffff;
    xduration_ms_last = 0;
}


void backlight::fade (uint16_t brightness_target1, uint32_t duration_ms){
    // feststellen ob eingeblendet oder ausgeblendet werden soll
    if (brightness_target1==brightness){                // Wert bereits erreicht, nichts zu tun bzw. fading abbrechen
        busy = 0;
        brightness_target = brightness_target1; // das auch dann gesetzt werden, wenn schon das Ziel erreicht wurde
        xduration_ms_last = duration_ms; // dito
    } else {
        if ((brightness_target!=brightness_target1) || (xduration_ms_last!=duration_ms)) { // only change ramp if brightness_target1 changed or duration changed
            brightness_target = brightness_target1;
            xduration_ms_last = duration_ms;
            brightness_started = brightness;
            xstarted_ms = millis();
            xend_ms = xstarted_ms+duration_ms;
            xduration_ms = duration_ms;
            next_update_stepsize = xduration_ms / 1023; // vereinfachte Berechnung um jeden Step ausnutzen zu können.
            next_update_ms = xstarted_ms+next_update_stepsize;
            busy = 1;
        }
    }
}


bool backlight::upd (void)
{

    bool value_changed = 0;
    if (busy&(millis()>next_update_ms)) { // Überspringen wenn der Zeitpunkt für Update noch nicht gekommen ist...
        if (millis()>xend_ms){ // fading abgeschlossen ?
            busy = 0;
            brightness = brightness_target;

        } else { // fading fortführen
            brightness = calc_brightness(xstarted_ms,xduration_ms,brightness_started,(int32_t (brightness_target)-int32_t (brightness_started)));
            next_update_ms = millis()+next_update_stepsize;
        }
        value_changed = (brightness==brightness_last) ? 0 : 1;
        brightness_last = brightness;
    }
    return value_changed;
}

uint16_t backlight::calc_brightness(uint32_t dxstart, uint32_t dx, uint16_t dystart, int16_t dy) {
    int64_t i64;
    uint32_t time_passed = millis() - dxstart;
    i64 = (int64_t)time_passed * (int64_t)dy;  // 64-Bit verhindert Überlauf
    i64 /= (int64_t)dx;
    i64 += (int64_t)dystart;
    if (i64 < 0) i64 = 0;
    if (i64 > 1023) i64 = 1023;
    return (uint16_t)i64;
}

/*
uint16_t backlight::calc_brightness_bu (uint32_t dxstart, uint32_t dx, uint16_t dystart, int16_t dy){
        int32_t i32;
		uint32_t time_passed;
		time_passed = millis()-uint32_t (dxstart);
        i32  = int32_t (time_passed) * int32_t (dy);
        i32 /= int32_t (dx);
        i32 += int32_t (dystart);
        if (i32<0) i32=0;
        if (i32>1023) i32=1023;
        return uint16_t (i32);
}
*/
