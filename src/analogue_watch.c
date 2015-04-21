#include <pebble.h>
#include <time.h>

#define Key_BackgroundColor 0
#define Key_Interval 1
#define Key_HourDigit 2
static int s_interval_second = 1, s_hourDigit_show = 1;
static GColor s_background_color = GColorClear;
static GColor s_foreground_color = GColorClear;

static Window *s_main_window;
static Layer *s_main_layer;
static int s_hand_angle;
static GPoint s_hand_shift;

typedef struct {
  int hour, minute, second;
} WatchTime;
static WatchTime s_current_time = {
  .hour = -2, .minute = -2, .second = -2
};
static bool s_force_update;

static const int DivisionLength = 120;
#define HandLength 114

static GPath *s_hand_path_ptr, *s_second_path_ptr;
static const GPathInfo HandPathInfo = {
  .num_points = 10,
  .points = (GPoint []) 
  {{-4, -HandLength}, {-3, -HandLength-3}, 
  {0,-HandLength-4}, {3, -HandLength-3}, {4, -HandLength},
  {4, 0}, {3, 3}, {0, 4}, {-3, 3}, {-4, 0}}
};

static const GPathInfo SecondPathInfo = {
  .num_points = 10,
  .points = (GPoint []) 
  {{-5, -HandLength}, {-4, -HandLength-4}, 
  {0,-HandLength-5}, {4, -HandLength-4}, {5, -HandLength},
  {5, 0}, {4, 4}, {0, 5}, {-4, 4}, {-5, 0}}
};

static void prepare_drawing_hand_division(Layer * const layer, GContext * const ctx,
  const int hour24, const int minute);
static void draw_hand_outline(Layer * const layer, GContext * const ctx);
static void draw_hand_filled(Layer * const layer, GContext * const ctx, const int second);
static void draw_division(Layer * const layer, GContext * const ctx,
        const int minute, const bool filled);
static void draw_divisions(Layer * const layer, GContext * const ctx,
			  const int hour24, const int minute);
static void draw_hour_digit(Layer * const Layer, GContext * const ctx,
        const int hour24, const int minute);
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);
static void read_persist_values(void);

// drawing

static void prepare_drawing_hand_division(Layer * const layer, GContext * const ctx,
  const int hour24, const int minute) {
  const GRect rect = layer_get_bounds(layer);

  s_hand_angle = minute * TRIG_MAX_ANGLE / 60;

  const int am_flag = (hour24 < 12) ? -1 : 1;
  const int hour12 = hour24 % 12;
  const int mh2 = minute * 2 + am_flag * hour12;
  const int mh2_angle = mh2 * TRIG_MAX_ANGLE / 2 / 60;
  int x =  - (int)(DivisionLength * 3 / 4 * sin_lookup(mh2_angle) / TRIG_MAX_RATIO)
      + rect.size.w / 2;
  int y =    (int)(DivisionLength * 3 / 4 * cos_lookup(mh2_angle) / TRIG_MAX_RATIO)
      + rect.size.h / 2;
  const int margin = -2;
  if (x < -margin) x = -margin;
  if (y < -margin) y = -margin;
  if (x >= rect.size.w + margin) x = rect.size.w + margin - 1;
  if (y >= rect.size.h + margin) y = rect.size.h + margin - 1;
  s_hand_shift = GPoint(x, y);

  graphics_context_set_fill_color(ctx, s_background_color);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void draw_hand_outline(Layer * const layer, GContext * const ctx) {
  graphics_context_set_stroke_color(ctx, s_foreground_color);
  gpath_rotate_to(s_hand_path_ptr, s_hand_angle);
  gpath_move_to(s_hand_path_ptr, s_hand_shift);
  gpath_draw_outline(ctx, s_hand_path_ptr);
}

static void draw_division(Layer * const layer, GContext * const ctx,
  const int minute, const bool filled) {
  graphics_context_set_stroke_color(ctx, s_foreground_color);
  graphics_context_set_fill_color(ctx, s_foreground_color);
  const int angle = minute * TRIG_MAX_ANGLE / 60;
  const int x = (int)(  DivisionLength * sin_lookup(angle) / TRIG_MAX_RATIO) + s_hand_shift.x;
  const int y = (int)(- DivisionLength * cos_lookup(angle) / TRIG_MAX_RATIO) + s_hand_shift.y;
  const int radius = (minute % 5 == 0) ? 5 : 2;
  if (filled)
    graphics_fill_circle(ctx, GPoint(x, y), radius);
  else
    graphics_draw_circle(ctx, GPoint(x, y), radius);
}

static void draw_hand_filled(Layer * const layer, GContext * const ctx, const int second) {
  const int len = second * HandLength / (60 - 1);
  const int pt_num = s_second_path_ptr->num_points / 2;

  for (int i = 0; i < pt_num; i ++)
    s_second_path_ptr->points[i].y = -len;

  graphics_context_set_fill_color(ctx, s_foreground_color);
  gpath_rotate_to(s_second_path_ptr, s_hand_angle);
  gpath_move_to(s_second_path_ptr, s_hand_shift);
  gpath_draw_filled(ctx, s_second_path_ptr);

  for (int i = 0; i < pt_num; i ++)
    s_second_path_ptr->points[i].y = +len;
}

static void draw_hour_digit(Layer * const Layer, GContext * const ctx,
        const int hour24, const int minute) {
  const int digit_dist = DivisionLength * 3 / 4;
  const int radius = 18;
  const int angle = minute * TRIG_MAX_ANGLE / 60;
  static char digit_str[] = "99";
  // circle
  const int cx = (int)(  digit_dist * sin_lookup(angle) / TRIG_MAX_RATIO) + s_hand_shift.x;
  const int cy = (int)(- digit_dist * cos_lookup(angle) / TRIG_MAX_RATIO) + s_hand_shift.y;
  graphics_context_set_fill_color(ctx, s_background_color);
  graphics_fill_circle(ctx, GPoint(cx, cy), radius);
  graphics_context_set_stroke_color(ctx, s_foreground_color);
  graphics_draw_circle(ctx, GPoint(cx, cy), radius);
  // digit
  snprintf(digit_str, sizeof(digit_str), "%2u", hour24);
  graphics_context_set_text_color(ctx, s_foreground_color);
  graphics_draw_text(ctx, digit_str, 
    fonts_get_system_font(FONT_KEY_GOTHIC_28),
    GRect(cx - radius * 2, cy - radius, radius * 4, radius * 2),
    GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_divisions(Layer * const layer, GContext * const ctx,
    const int hour24, const int minute) {
  const int hour12 = hour24 % 12;
  const bool is_am = hour24 < 12;
  if (is_am) {
    for (int i = minute - 14; i < minute - hour12 + 1; ++ i)
      draw_division(layer, ctx, (i + 60) % 60, false);
    for (int i = minute - hour12 + 1; i < minute; ++ i)
      draw_division(layer, ctx, (i + 60) % 60, true);
    draw_division(layer, ctx, minute, hour12 > 0);
    for (int i = minute + 1; i <= minute + 14; ++ i)
      draw_division(layer, ctx, i % 60, false);
  } else {
    for (int i = minute - 14; i < minute; ++ i)
      draw_division(layer, ctx, i >= 0 ? i : i + 60, false);
    draw_division(layer, ctx, minute, hour12 > 0);
    for (int i = minute + 1; i <= minute + hour12 - 1; ++ i)
      draw_division(layer, ctx, i % 60, true);
    for (int i = minute + hour12; i <= minute + 14; ++ i)
      draw_division(layer, ctx, i % 60, false);
 }
}

static void update_main_layer(Layer *layer, GContext *ctx) {
  static WatchTime last_time = {
    .hour = -1, .minute = -1, .second = -1
  };
  if (s_current_time.hour < 0) return;
  //APP_LOG(APP_LOG_LEVEL_INFO, "update_main_layer()");

  if (s_force_update ||
      (last_time.minute != s_current_time.minute) ||
      (last_time.hour != s_current_time.hour)) {
    prepare_drawing_hand_division(layer, ctx,
      s_current_time.hour, s_current_time.minute);
    draw_hand_outline(layer, ctx);
    draw_divisions(layer, ctx, s_current_time.hour, s_current_time.minute);
    if (s_interval_second)
      draw_hand_filled(layer, ctx, s_current_time.second);
    else
      draw_hand_filled(layer, ctx, 60 - 1);
  } else if (last_time.second != s_current_time.second) {
    draw_hand_filled(layer, ctx, s_current_time.second);
  }
  if (s_hourDigit_show)
    draw_hour_digit(layer, ctx, s_current_time.hour, s_current_time.minute);

  last_time = s_current_time;
  s_force_update = false;
}

static void create_rect_pathes(void) {
  s_hand_path_ptr = gpath_create(&HandPathInfo);
  s_second_path_ptr = gpath_create(&SecondPathInfo);
}

static void destroy_rect_pathes(void) {
  if (s_hand_path_ptr)
    gpath_destroy(s_hand_path_ptr);
  if (s_second_path_ptr)
    gpath_destroy(s_second_path_ptr);
}

static void main_window_load(Window *window) {
  s_main_layer =  window_get_root_layer(window);
  if (s_main_layer)
    layer_set_update_proc(s_main_layer, update_main_layer);
}
static void main_window_unload(Window *window) {}

// event handler

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_current_time.hour = tick_time->tm_hour;
  s_current_time.minute = tick_time->tm_min;
  s_current_time.second = tick_time->tm_sec;
  layer_mark_dirty(s_main_layer);
}

static void update_now(void) {
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  tick_handler(lt, false);
}

// settings

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *t = dict_read_first(iterator);
  while (t != NULL) {
    switch (t->key) {
    case Key_BackgroundColor:
      APP_LOG(APP_LOG_LEVEL_INFO, "bgcolor_white %d", (int)t->value->int32);
      persist_write_int(Key_BackgroundColor, (int)t->value->int32);
      break;
    case Key_Interval:
      APP_LOG(APP_LOG_LEVEL_INFO, "interval_second %d", (int)t->value->int32);
      persist_write_int(Key_Interval, (int)t->value->int32);
      break;
    case Key_HourDigit:
      APP_LOG(APP_LOG_LEVEL_INFO, "hour_digit_show %d", (int)t->value->int32);
      persist_write_int(Key_HourDigit, (int)t->value->int32);
      break;
      default:
      APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!!", (int)t->key);
      break;
    }
    t = dict_read_next(iterator);
  }
  read_persist_values();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}
static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}
static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void read_persist_values(void) {
  if (persist_read_int(Key_BackgroundColor)) {
    // background color is white
    s_background_color = GColorWhite;
    s_foreground_color = GColorBlack;
  } else {
    // background color is black
    s_background_color = GColorBlack;
    s_foreground_color = GColorWhite;
  }
  s_interval_second = persist_read_int(Key_Interval);
  tick_timer_service_subscribe(
    s_interval_second ? SECOND_UNIT : MINUTE_UNIT,
    tick_handler);

  s_hourDigit_show = persist_read_int(Key_HourDigit);
  s_force_update = true;
}

// main

static void init(void) {
  s_force_update = true;
  create_rect_pathes();
  s_main_window = window_create();
  if (!s_main_window) return;
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  read_persist_values();
  update_now();

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit(void) {
  destroy_rect_pathes();
  if (s_main_window)
    window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
