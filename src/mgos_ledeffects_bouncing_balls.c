#include "mgos.h"
#include "led_master.h"

typedef struct {
    int count;
    double* height;
    int* start_height;
    double* impact_velocity;
    double* impact_velocity_start;
    double* time_since_last_bounce;
    long* clock_time_since_last_bounce;
    double* dampening;
    int* position;
    int column;
    tools_rgb_data* colors;
} bounce_data;

static bounce_data* main_bd;
static double gravity = -9.81;

static void mgos_intern_bouncing_balls_get_colors(mgos_rgbleds* leds, char* anim, bounce_data* bd, int ball_pos)
{
    size_t elems;
    int i, ball = 0;
    char** colors = tools_config_get_dyn_arr("ledeffects.%s.colors", anim, &elems);
    if (colors) {
        for (i = 0; i < elems - 2; i += 3) {
            if (ball < bd->count) {
                bd->colors[ball].r = colors[i][0];
                bd->colors[ball].g = colors[i][1];
                bd->colors[ball].b = colors[i][2];
                ball++;
            } else {
                break;
            }
        }
    }
    // fill up with random colors, if configred colors amount
    // is smaller then the amount of balls
    if (colors == NULL || elems < bd->count) {
        while (ball < bd->count) {
            if (ball_pos == 0) {
                bd->colors[ball] = (tools_rgb_data)tools_get_random_color(bd->colors[ball], bd->colors, bd->count, 60.0);
            } else if (ball_pos == ball) {
                // if ball_pos is set, only the passed index will be set with new colors
                bd->colors[ball] = (tools_rgb_data)tools_get_random_color(bd->colors[ball], bd->colors, bd->count, 60.0);
            }
            ball++;
        }
    }
    tools_str_split_free(colors, elems);
}

static void mgos_intern_bouncing_balls_set(mgos_rgbleds* leds, bounce_data* curr_bd, int ball)
{
    for (int i = 0; i < curr_bd->count; i++) {
        if (ball == -1 || ball == i) {
            curr_bd->clock_time_since_last_bounce[i] = (mgos_uptime_micros() / 1000);
            curr_bd->start_height[i] = tools_get_random(1, leds->panel_height / 2) + i;
            curr_bd->height[i] = curr_bd->start_height[i];
            curr_bd->position[i] = 0;
            curr_bd->impact_velocity_start[i] = sqrt(-2 * gravity * curr_bd->start_height[i]);
            curr_bd->impact_velocity[i] = curr_bd->impact_velocity_start[i];
            curr_bd->time_since_last_bounce[i] = 0;
            curr_bd->dampening[i] = (tools_get_random(88, 90) / 100.0) - (i * 1.0) / pow(curr_bd->count, 2);
            mgos_intern_bouncing_balls_get_colors(leds, "bouncing_balls", curr_bd, i);
        }
    }
}

static bounce_data* mgos_intern_bouncing_balls_init(mgos_rgbleds* leds)
{
    int num_columns = leds->panel_width;
    bounce_data* result = calloc(num_columns, sizeof(bounce_data));

    leds->timeout = mgos_sys_config_get_ledeffects_bouncing_balls_timeout();
    leds->dim_all = mgos_sys_config_get_ledeffects_bouncing_balls_dim_all();

    for (int col = 0; col < num_columns; col++) {

        bounce_data* curr_bd = &result[col];

        curr_bd->count = mgos_sys_config_get_ledeffects_bouncing_balls_count();
        curr_bd->height = calloc(sizeof(double), curr_bd->count);
        curr_bd->impact_velocity = calloc(sizeof(double), curr_bd->count);
        curr_bd->time_since_last_bounce = calloc(sizeof(double), curr_bd->count);
        curr_bd->position = calloc(sizeof(int), curr_bd->count);
        curr_bd->start_height = calloc(sizeof(int), curr_bd->count);
        curr_bd->impact_velocity_start = calloc(sizeof(double), curr_bd->count);
        curr_bd->clock_time_since_last_bounce = calloc(sizeof(long), curr_bd->count);
        curr_bd->dampening = calloc(sizeof(double), curr_bd->count);
        curr_bd->colors = calloc(sizeof(tools_rgb_data), curr_bd->count);
        curr_bd->column = col;
        mgos_intern_bouncing_balls_set(leds, curr_bd, -1);
    }

    return result;
}

static void mgos_intern_bouncing_balls_exit(mgos_rgbleds* leds)
{
   int num_columns = leds->panel_width;

    for (int col = 0; col < num_columns; col++) {
        bounce_data* curr_bd = &main_bd[col];

        if (curr_bd->height) {
            free(curr_bd->height);
        }
        if (curr_bd->impact_velocity) {
            free(curr_bd->impact_velocity);
        }
        if (curr_bd->time_since_last_bounce) {
            free(curr_bd->time_since_last_bounce);
        }
        if (curr_bd->position) {
            free(curr_bd->position);
        }
        if (curr_bd->start_height) {
            free(curr_bd->start_height);
        }
        if (curr_bd->impact_velocity_start) {
            free(curr_bd->impact_velocity_start);
        }
        if (curr_bd->clock_time_since_last_bounce) {
            free(curr_bd->clock_time_since_last_bounce);
        }
        if (curr_bd->dampening) {
            free(curr_bd->dampening);
        }
        if (curr_bd->colors) {
            free(curr_bd->colors);
        }
    }

    free(main_bd);
    main_bd = NULL;
}

static void mgos_intern_bouncing_balls_loop(mgos_rgbleds* leds) {
    uint32_t num_rows = leds->panel_height;
    uint32_t num_columns = leds->panel_width;

    int bounces = mgos_sys_config_get_ledeffects_bouncing_balls_loops();

    while (bounces--) {
        for (int col = 0; col < num_columns; col++) {
            bounce_data* curr_bd = &main_bd[col];

            for (int ball = 0; ball < curr_bd->count; ball++) {

                curr_bd->time_since_last_bounce[ball] = (mgos_uptime_micros() / 1000) - curr_bd->clock_time_since_last_bounce[ball];
                curr_bd->height[ball] = 0.5 * gravity * pow(curr_bd->time_since_last_bounce[ball] / 1000, 2.0) + curr_bd->impact_velocity[ball] * curr_bd->time_since_last_bounce[ball] / 1000;

                if (curr_bd->height[ball] < 0) {
                    curr_bd->height[ball] = 0;
                    curr_bd->impact_velocity[ball] = curr_bd->dampening[ball] * curr_bd->impact_velocity[ball];
                    curr_bd->clock_time_since_last_bounce[ball] = (mgos_uptime_micros() / 1000);
                    if (curr_bd->impact_velocity[ball] < 0.01) {
                        curr_bd->impact_velocity[ball] = curr_bd->impact_velocity_start[ball];
                    }
                }
                curr_bd->position[ball] = round(curr_bd->height[ball] * ((num_rows >= curr_bd->count ? num_rows : curr_bd->count) - 1) / curr_bd->start_height[ball]);

                if (curr_bd->height[ball] < 0.01 && curr_bd->time_since_last_bounce[ball] > 0.0 && curr_bd->impact_velocity[ball] < 0.02) {
                    mgos_intern_bouncing_balls_set(leds, curr_bd, ball);
                }
            }

            for (int i = 0; i < curr_bd->count; i++) {
                if (curr_bd->position[i] < num_rows) {
                    tools_rgb_data out_pix = curr_bd->colors[i];
                    mgos_universal_led_plot_pixel(leds, col, leds->panel_height - 1 - curr_bd->position[i], out_pix, false);
                }
            }
        }

        mgos_universal_led_show(leds);
        mgos_universal_clear(leds);
        mgos_wdt_feed();
    }
}

void mgos_ledeffects_bouncing_balls(void* param, mgos_rgbleds_action action)
{
    static bool do_time = false;
    static uint32_t max_time = 0;
    uint32_t time = (mgos_uptime_micros() / 1000);
    mgos_rgbleds* leds = (mgos_rgbleds*)param;

    switch (action) {
    case MGOS_RGBLEDS_ACT_INIT:
        LOG(LL_INFO, ("mgos_ledeffects_bouncing_balls: called (init)"));
        main_bd = mgos_intern_bouncing_balls_init(leds);
        break;
    case MGOS_RGBLEDS_ACT_EXIT:
        LOG(LL_INFO, ("mgos_ledeffects_bouncing_balls: called (exit)"));
        mgos_intern_bouncing_balls_exit(leds);
        break;
    case MGOS_RGBLEDS_ACT_LOOP:
        LOG(LL_VERBOSE_DEBUG, ("mgos_ledeffects_bouncing_balls: called (loop)"));
        mgos_intern_bouncing_balls_loop(leds);
        if (do_time) {
            time = (mgos_uptime_micros() /1000) - time;
            max_time = (time > max_time) ? time : max_time;
            LOG(LL_VERBOSE_DEBUG, ("Bouncing balls loop duration: %d milliseconds, max: %d ...", time / 1000, max_time / 1000));
        }
        break;
    }
}

bool mgos_bouncing_balls_init(void) {
  LOG(LL_INFO, ("mgos_bouncing_balls_init ..."));
  ledmaster_add_effect("ANIM_BOUNCING_BALLS", mgos_ledeffects_bouncing_balls);
  return true;
}
