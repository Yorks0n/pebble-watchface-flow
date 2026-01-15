#include <pebble.h>

static Window *s_window;
static Layer *s_canvas_layer;
static AppTimer *s_timer;
static int32_t s_phase;

enum {
  kLineWidth = 2,
  kGap = 4,
  kFps = 15,
  kSpatialPeriodMultiplier = 2
};

static int16_t s_line_count;
static int16_t s_amp;
static int16_t s_base_len;
static int32_t s_top_phase_offset;
static int32_t s_bottom_phase_offset;

static void prv_tick(void *context);

static void prv_canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_fill_color(ctx, GColorWhite);

  const int16_t step = kLineWidth + kGap;

  for (int16_t i = 0; i < s_line_count; i++) {
    const int16_t x = i * step;
    const int32_t spatial_phase = (TRIG_MAX_ANGLE * (int32_t)x) /
      ((int32_t)bounds.size.w * kSpatialPeriodMultiplier);
    const bool is_top = (i % 2 == 0);
    const int32_t phase_offset = is_top ? s_top_phase_offset : s_bottom_phase_offset;
    const int32_t phase = s_phase + spatial_phase + phase_offset;
    const int16_t dy = (int32_t)sin_lookup(phase) * s_amp / TRIG_MAX_RATIO;
    int16_t length = s_base_len + dy;
    const int16_t bias_top = (bounds.size.w - x) / 16;
    const int16_t bias_bottom = x / 16;
    if (length < 0) {
      length = 0;
    } else if (length > bounds.size.h) {
      length = bounds.size.h;
    }

    if (is_top) {
      length += bias_top;
      if (length > bounds.size.h) {
        length = bounds.size.h;
      }
      graphics_fill_rect(ctx, GRect(x, 0, kLineWidth, length), 0, GCornerNone);
    } else {
      length += bias_bottom;
      if (length > bounds.size.h) {
        length = bounds.size.h;
      }
      graphics_fill_rect(ctx, GRect(x, bounds.size.h - length, kLineWidth, length), 0, GCornerNone);
    }
  }
}

static void prv_tick(void *context) {
  s_phase += TRIG_MAX_ANGLE / 64;
  if (s_phase >= TRIG_MAX_ANGLE) {
    s_phase -= TRIG_MAX_ANGLE;
  }

  layer_mark_dirty(s_canvas_layer);
  s_timer = app_timer_register(1000 / kFps, prv_tick, NULL);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, prv_canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  const int16_t step = kLineWidth + kGap;
  s_line_count = bounds.size.w / step + 1;
  s_amp = bounds.size.h / 8;
  s_base_len = (bounds.size.h * 3) / 4;
  s_phase = 0;
  s_top_phase_offset = 0;
  s_bottom_phase_offset = TRIG_MAX_ANGLE / 4;
  s_timer = app_timer_register(1000 / kFps, prv_tick, NULL);
}

static void prv_window_unload(Window *window) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  layer_destroy(s_canvas_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
