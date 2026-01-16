#include <pebble.h>
#include <stdlib.h>
#include <string.h>
#include "message_keys.auto.h"

static Window *s_window;
static Layer *s_canvas_layer;
static AppTimer *s_timer;
static int32_t s_phase;
static char s_time_text[6];
static GColor s_background_color;
static GColor s_foreground_color;
static int s_theme;
static int s_time_format;
static int s_animation_frequency;
static int s_animation_duration_sec;
static int16_t s_animation_frames_remaining;

static void prv_tick(void *context);
static void prv_start_animation(void);
static void prv_stop_animation(void);

enum {
  kLineWidth = 2,
  kGap = 4,
  kFps = 15,
  kSpatialPeriodMultiplier = 2,
  kGlyphRows = 10,
  kGlyphSpacing = 1
};

static int16_t s_line_count;
static int16_t s_amp;
static int16_t s_base_len;
static int32_t s_top_phase_offset;
static int32_t s_bottom_phase_offset;

enum {
  THEME_DARK = 0,
  THEME_LIGHT = 1,
  THEME_WARM = 2,
  THEME_NATURAL = 3,
  THEME_COOL = 4,
  THEME_DUSK = 5,
};

enum {
  ANIM_FREQ_OFF = 0,
  ANIM_FREQ_EVERY_MINUTE = 1,
  ANIM_FREQ_EVERY_15_MINUTES = 2,
  ANIM_FREQ_EVERY_30_MINUTES = 3,
  ANIM_FREQ_EVERY_HOUR = 4,
  ANIM_FREQ_ALWAYS_ON = 5,
};

enum {
  PERSIST_KEY_THEME = 1,
  PERSIST_KEY_TIME_FORMAT = 2,
  PERSIST_KEY_ANIM_FREQUENCY = 3,
  PERSIST_KEY_ANIM_DURATION = 4,
};

typedef struct {
  const char *rows[kGlyphRows];
} DigitGlyph;

static const DigitGlyph s_digit_glyphs[] = {
  // 0
  { .rows = {
      "011110",
      "111111",
      "110011",
      "110011",
      "110011",
      "110011",
      "110011",
      "110011",
      "111111",
      "011110",
    } },
  // 1
  { .rows = {
      "011100",
      "011100",
      "001100",
      "001100",
      "001100",
      "001100",
      "001100",
      "001100",
      "011110",
      "011110",
    } },
  // 2
  { .rows = {
      "011111",
      "111111",
      "110011",
      "000011",
      "000110",
      "001100",
      "011000",
      "110000",
      "111111",
      "111111",
    } },
  // 3
  { .rows = {
      "011100",
      "111111",
      "110011",
      "000011",
      "001110",
      "001110",
      "000011",
      "110011",
      "111111",
      "011110",
    } },
  // 4
  { .rows = {
      "000011",
      "000111",
      "001111",
      "011011",
      "110011",
      "110011",
      "111111",
      "111111",
      "000011",
      "000011",
    } },
  // 5
  { .rows = {
      "111111",
      "111111",
      "110000",
      "110000",
      "111100",
      "001111",
      "000011",
      "110011",
      "111111",
      "011110",
    } },
  // 6
  { .rows = {
      "011110",
      "111111",
      "110011",
      "110000",
      "111100",
      "111111",
      "110011",
      "110011",
      "111111",
      "011110",
    } },
  // 7
  { .rows = {
      "111111",
      "111111",
      "000011",
      "000011",
      "000111",
      "001110",
      "001110",
      "001100",
      "001100",
      "001100",
    } },
  // 8
  { .rows = {
      "011110",
      "111111",
      "110011",
      "110011",
      "011110",
      "011110",
      "110011",
      "110011",
      "111111",
      "011110",
    } },
  // 9
  { .rows = {
      "011110",
      "111111",
      "110011",
      "110011",
      "111111",
      "001111",
      "000011",
      "110011",
      "111111",
      "011110",
    } },
  // :
  { .rows = {
      "0000",
      "0000",
      "0110",
      "0110",
      "0000",
      "0000",
      "0000",
      "0000",
      "0110",
      "0110",
    } },
};

static void prv_update_time(struct tm *tick_time) {
  if (s_time_format == 0) {
    strftime(s_time_text, sizeof(s_time_text), "%H:%M", tick_time);
  } else {
    strftime(s_time_text, sizeof(s_time_text), "%I:%M", tick_time);
    if (s_time_text[0] == '0') {
      memmove(s_time_text, s_time_text + 1, sizeof(s_time_text) - 1);
    }
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  prv_update_time(tick_time);
  layer_mark_dirty(s_canvas_layer);

  if (s_animation_frequency == ANIM_FREQ_OFF) {
    return;
  }
  if (s_animation_frequency == ANIM_FREQ_ALWAYS_ON) {
    prv_start_animation();
    return;
  }

  const int minute = tick_time->tm_min;
  bool should_trigger = false;
  switch (s_animation_frequency) {
    case ANIM_FREQ_EVERY_MINUTE:
      should_trigger = true;
      break;
    case ANIM_FREQ_EVERY_15_MINUTES:
      should_trigger = (minute % 15 == 0);
      break;
    case ANIM_FREQ_EVERY_30_MINUTES:
      should_trigger = (minute % 30 == 0);
      break;
    case ANIM_FREQ_EVERY_HOUR:
      should_trigger = (minute == 0);
      break;
    case ANIM_FREQ_ALWAYS_ON:
      should_trigger = true;
      break;
    case ANIM_FREQ_OFF:
    default:
      should_trigger = false;
      break;
  }

  if (should_trigger) {
    prv_start_animation();
  }
}

static void apply_theme(void) {
  if (s_theme < THEME_DARK || s_theme > THEME_DUSK) {
    s_theme = THEME_DARK;
  }

#ifdef PBL_COLOR
  switch (s_theme) {
    case THEME_LIGHT:
      s_background_color = GColorWhite;
      s_foreground_color = GColorBlack;
      break;
    case THEME_WARM:
      s_background_color = GColorOrange;
      s_foreground_color = GColorPastelYellow;
      break;
    case THEME_NATURAL:
      s_background_color = GColorDarkGreen;
      s_foreground_color = GColorMintGreen;
      break;
    case THEME_COOL:
      s_background_color = GColorDukeBlue;
      s_foreground_color = GColorCeleste;
      break;
    case THEME_DUSK:
      s_background_color = GColorImperialPurple;
      s_foreground_color = GColorRajah;
      break;
    case THEME_DARK:
    default:
      s_background_color = GColorBlack;
      s_foreground_color = GColorWhite;
      break;
  }
#else
  if (s_theme > THEME_LIGHT) {
    s_theme = THEME_DARK;
  }
  if (s_theme == THEME_LIGHT) {
    s_background_color = GColorWhite;
    s_foreground_color = GColorBlack;
  } else {
    s_background_color = GColorBlack;
    s_foreground_color = GColorWhite;
  }
#endif

  window_set_background_color(s_window, s_background_color);
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void send_settings_to_phone(void) {
  DictionaryIterator *iter = NULL;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK || !iter) {
    return;
  }
  dict_write_int(iter, MESSAGE_KEY_theme, &s_theme, sizeof(s_theme), true);
  dict_write_int(iter, MESSAGE_KEY_time_format, &s_time_format, sizeof(s_time_format), true);
  dict_write_int(iter, MESSAGE_KEY_animation_frequency, &s_animation_frequency,
                 sizeof(s_animation_frequency), true);
  dict_write_int(iter, MESSAGE_KEY_animation_duration, &s_animation_duration_sec,
                 sizeof(s_animation_duration_sec), true);
  app_message_outbox_send();
}

static int prv_tuple_to_int(const Tuple *tuple) {
  if (tuple->type == TUPLE_CSTRING) {
    return atoi(tuple->value->cstring);
  } else if (tuple->type == TUPLE_UINT) {
    return (int)tuple->value->uint32;
  }
  return (int)tuple->value->int32;
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *theme_tuple = dict_find(iter, MESSAGE_KEY_theme);
  Tuple *theme_request_tuple = dict_find(iter, MESSAGE_KEY_theme_request);
  Tuple *time_format_tuple = dict_find(iter, MESSAGE_KEY_time_format);
  Tuple *time_format_request_tuple = dict_find(iter, MESSAGE_KEY_time_format_request);
  Tuple *anim_frequency_tuple = dict_find(iter, MESSAGE_KEY_animation_frequency);
  Tuple *anim_frequency_request_tuple = dict_find(iter, MESSAGE_KEY_animation_frequency_request);
  Tuple *anim_duration_tuple = dict_find(iter, MESSAGE_KEY_animation_duration);
  Tuple *anim_duration_request_tuple = dict_find(iter, MESSAGE_KEY_animation_duration_request);
  const bool requested = theme_request_tuple || time_format_request_tuple ||
    anim_frequency_request_tuple || anim_duration_request_tuple;
  bool did_update_theme = false;
  bool did_update_time_format = false;
  bool did_update_animation = false;

  if (theme_tuple) {
    int new_theme = prv_tuple_to_int(theme_tuple);
#ifdef PBL_COLOR
    if (new_theme >= THEME_DARK && new_theme <= THEME_DUSK) {
      s_theme = new_theme;
    }
#else
    if (new_theme == THEME_DARK || new_theme == THEME_LIGHT) {
      s_theme = new_theme;
    }
#endif

    persist_write_int(PERSIST_KEY_THEME, s_theme);
    apply_theme();
    did_update_theme = true;
  }

  if (time_format_tuple) {
    int new_time_format = prv_tuple_to_int(time_format_tuple);
    if (new_time_format == 0 || new_time_format == 1) {
      s_time_format = new_time_format;
    }
    persist_write_int(PERSIST_KEY_TIME_FORMAT, s_time_format);
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    prv_update_time(t);
    if (s_canvas_layer) {
      layer_mark_dirty(s_canvas_layer);
    }
    did_update_time_format = true;
  }

  if (anim_frequency_tuple) {
    int new_frequency = prv_tuple_to_int(anim_frequency_tuple);
    if (new_frequency >= ANIM_FREQ_OFF && new_frequency <= ANIM_FREQ_ALWAYS_ON) {
      s_animation_frequency = new_frequency;
    }
    persist_write_int(PERSIST_KEY_ANIM_FREQUENCY, s_animation_frequency);
    if (s_animation_frequency == ANIM_FREQ_OFF) {
      prv_stop_animation();
    } else if (s_animation_frequency == ANIM_FREQ_ALWAYS_ON) {
      prv_start_animation();
    }
    did_update_animation = true;
  }

  if (anim_duration_tuple) {
    int new_duration = prv_tuple_to_int(anim_duration_tuple);
    if (new_duration >= 1 && new_duration <= 3) {
      s_animation_duration_sec = new_duration;
    }
    persist_write_int(PERSIST_KEY_ANIM_DURATION, s_animation_duration_sec);
    if (s_animation_frames_remaining > 0) {
      s_animation_frames_remaining = s_animation_duration_sec * kFps;
    }
    did_update_animation = true;
  }

  if (requested || did_update_theme || did_update_time_format || did_update_animation) {
    send_settings_to_phone();
  }
}

static int prv_glyph_index(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch == ':') {
    return 10;
  }
  return -1;
}

static int16_t prv_glyph_width(const DigitGlyph *glyph) {
  return (int16_t)strlen(glyph->rows[0]);
}

static void prv_canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, s_background_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_fill_color(ctx, s_foreground_color);

  const int16_t step = kLineWidth + kGap;

  for (int16_t i = 0; i < s_line_count; i++) {
    const int16_t x = i * step + 2;
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

  int16_t total_blocks = 0;
  const size_t time_len = strlen(s_time_text);
  for (size_t i = 0; i < time_len; i++) {
    const int glyph_index = prv_glyph_index(s_time_text[i]);
    if (glyph_index < 0) {
      continue;
    }
    total_blocks += prv_glyph_width(&s_digit_glyphs[glyph_index]);
    if (i + 1 < time_len) {
      total_blocks += kGlyphSpacing;
    }
  }

  const int16_t block_size = bounds.size.w > 192 ? 6 : 4;
  const int16_t total_width = total_blocks * block_size;
  const int16_t total_height = kGlyphRows * block_size;
  const int16_t origin_x = (bounds.size.w - total_width) / 2;
  const int16_t origin_y = (bounds.size.h - total_height) / 2;

  graphics_context_set_fill_color(ctx, s_background_color);
  int16_t cursor_x = origin_x;
  for (size_t i = 0; i < time_len; i++) {
    const int glyph_index = prv_glyph_index(s_time_text[i]);
    if (glyph_index < 0) {
      continue;
    }
    const DigitGlyph *glyph = &s_digit_glyphs[glyph_index];
    const int16_t glyph_width = prv_glyph_width(glyph);

    for (int16_t row = 0; row < kGlyphRows; row++) {
      for (int16_t col = 0; col < glyph_width; col++) {
        if (glyph->rows[row][col] == '1') {
          const int16_t px = cursor_x + col * block_size;
          const int16_t py = origin_y + row * block_size;
          graphics_fill_rect(ctx, GRect(px, py, block_size, block_size),
                             0, GCornerNone);
        }
      }
    }
    cursor_x += (glyph_width + kGlyphSpacing) * block_size;
  }
}

static void prv_tick(void *context) {
  if (s_animation_frames_remaining == 0) {
    s_timer = NULL;
    return;
  }

  s_phase += TRIG_MAX_ANGLE / 64;
  if (s_phase >= TRIG_MAX_ANGLE) {
    s_phase -= TRIG_MAX_ANGLE;
  }

  layer_mark_dirty(s_canvas_layer);
  if (s_animation_frames_remaining > 0) {
    s_animation_frames_remaining--;
  }
  if (s_animation_frames_remaining != 0) {
    s_timer = app_timer_register(1000 / kFps, prv_tick, NULL);
  } else {
    s_timer = NULL;
  }
}

static void prv_start_animation(void) {
  if (s_animation_duration_sec <= 0 && s_animation_frequency != ANIM_FREQ_ALWAYS_ON) {
    return;
  }
  if (s_animation_frequency == ANIM_FREQ_ALWAYS_ON) {
    s_animation_frames_remaining = -1;
  } else {
    s_animation_frames_remaining = s_animation_duration_sec * kFps;
  }
  if (!s_timer) {
    s_timer = app_timer_register(1000 / kFps, prv_tick, NULL);
  }
}

static void prv_stop_animation(void) {
  s_animation_frames_remaining = 0;
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
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

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  prv_update_time(t);
  layer_mark_dirty(s_canvas_layer);
  if (s_animation_frequency == ANIM_FREQ_ALWAYS_ON) {
    prv_start_animation();
  }
}

static void prv_window_unload(Window *window) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  layer_destroy(s_canvas_layer);
}

static void prv_init(void) {
  s_theme = THEME_DARK;
  if (persist_exists(PERSIST_KEY_THEME)) {
    s_theme = persist_read_int(PERSIST_KEY_THEME);
  }
  s_time_format = clock_is_24h_style() ? 0 : 1;
  if (persist_exists(PERSIST_KEY_TIME_FORMAT)) {
    s_time_format = persist_read_int(PERSIST_KEY_TIME_FORMAT);
  }
  s_animation_frequency = ANIM_FREQ_EVERY_MINUTE;
  if (persist_exists(PERSIST_KEY_ANIM_FREQUENCY)) {
    s_animation_frequency = persist_read_int(PERSIST_KEY_ANIM_FREQUENCY);
  }
  if (s_animation_frequency < ANIM_FREQ_OFF || s_animation_frequency > ANIM_FREQ_ALWAYS_ON) {
    s_animation_frequency = ANIM_FREQ_EVERY_MINUTE;
  }
  s_animation_duration_sec = 2;
  if (persist_exists(PERSIST_KEY_ANIM_DURATION)) {
    s_animation_duration_sec = persist_read_int(PERSIST_KEY_ANIM_DURATION);
  }
  if (s_animation_duration_sec < 1 || s_animation_duration_sec > 3) {
    s_animation_duration_sec = 2;
  }
  s_animation_frames_remaining = 0;

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);

  apply_theme();
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(64, 64);
  send_settings_to_phone();

  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
