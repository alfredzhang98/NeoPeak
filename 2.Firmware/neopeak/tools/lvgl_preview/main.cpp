#include <math.h>

#include <SDL2/SDL.h>

#include "lvgl.h"

#include "ui_app.h"

int main()
{
    lv_init();

    lv_display_t *display = lv_sdl_window_create(240, 240);
    if (display == nullptr) {
        return 1;
    }

    lv_sdl_mouse_create();
    lv_sdl_mousewheel_create();
    lv_sdl_keyboard_create();

    ui_app_create(lv_screen_active(), nullptr);

    float t = 0.0f;
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                return 0;
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                switch (event.key.keysym.sym) {
                case SDLK_UP:
                case SDLK_LEFT:
                    ui_app_handle_encoder_rotate(-1);
                    break;
                case SDLK_DOWN:
                case SDLK_RIGHT:
                    ui_app_handle_encoder_rotate(1);
                    break;
                case SDLK_RETURN:
                case SDLK_SPACE:
                    ui_app_handle_encoder_button(false, SDL_GetTicks());
                    break;
                case SDLK_BACKSPACE:
                    ui_app_handle_encoder_button(false, SDL_GetTicks());
                    ui_app_handle_encoder_button(false, SDL_GetTicks() + 1);
                    break;
                default:
                    break;
                }
            }
        }

        float roll = sinf(t) * 0.65f;
        float pitch = cosf(t * 0.7f) * 0.45f;
        float yaw = sinf(t * 0.35f) * 0.9f;
        float cr = cosf(roll * 0.5f);
        float sr = sinf(roll * 0.5f);
        float cp = cosf(pitch * 0.5f);
        float sp = sinf(pitch * 0.5f);
        float cy = cosf(yaw * 0.5f);
        float sy = sinf(yaw * 0.5f);
        float q[4] = {
            cy * cp * cr + sy * sp * sr,
            cy * cp * sr - sy * sp * cr,
            cy * sp * cr + sy * cp * sr,
            sy * cp * cr - cy * sp * sr,
        };
        ui_app_handle_imu(roll, pitch, yaw, q);

        lv_timer_handler();
        lv_delay_ms(5);
        t += 0.02f;
    }

    return 0;
}
