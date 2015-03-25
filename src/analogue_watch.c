#include <pebble.h>
#include <math.h>
#include <time.h>

static const float PI = 3.14159265359f;

static Window *s_main_window;
static Layer *s_main_layer;
static int s_hand_angle;
static GPoint s_hand_shift;

typedef struct {
  int hour, minute, second;
} WatchTime;
static WatchTime s_current_time;

#define HandLength 120
static const int ScreenWidth = 144;
static GPath *s_hand_path_ptr, *s_second_path_ptr;
static const GPathInfo HandPathInfo = {
  .num_points = 4,
  .points = (GPoint []) {{-4, -HandLength}, {4, -HandLength},
    {4, 0}, {-4, 0}}
};

static const GPathInfo SecondPathInfo = {
  .num_points = 4,
  .points = (GPoint []) {{-5, -HandLength}, {5, -HandLength},
    {5, 0}, {-5, 0}}
};

static void prepare_drawing_hand_division(Layer * const layer, GContext * const ctx, 
  const int hour24, const int minute);
static void draw_hand_outline(Layer * const layer, GContext * const ctx);
static void draw_hand_filled(Layer * const layer, GContext * const ctx, const int second);
static void draw_division(Layer * const layer, GContext * const ctx, const int minute, const bool filled);
static void draw_divisions(Layer * const layer, GContext * const ctx,
			   const int hour24, const int minute);
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);


// drawing

static void prepare_drawing_hand_division(Layer * const layer, GContext * const ctx, 
  const int hour24, const int minute) {
  s_hand_angle = minute * TRIG_MAX_ANGLE / 60;

  const int am_flag = (hour24 < 12) ? -1 : 1;
  const int hour12 = hour24 % 12;
  const float mh = minute + am_flag * hour12 / 2.0f;
  const float angle = 2 * PI * mh / 60;
  int x =  - (int)(HandLength * 4 / 5 * sin(angle)) + ScreenWidth / 2;
  int y =    (int)(HandLength * 4 / 5 * cos(angle)) + ScreenWidth / 2;
//  APP_LOG(APP_LOG_LEVEL_INFO, "minute=%d angle=%d x=%d y=%d", minute, (int)(angle * 100), x, y);
  static const int margin = ScreenWidth - HandLength;
  if (x < margin) x = margin;
  if (y < margin) y = margin;
  if (x >= ScreenWidth - margin) x = ScreenWidth - margin + 1;
  if (y >= ScreenWidth - margin) y = ScreenWidth - margin + 1;
  s_hand_shift = GPoint(x, y);
  
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void draw_hand_outline(Layer * const layer, GContext * const ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  gpath_rotate_to(s_hand_path_ptr, s_hand_angle);
  gpath_move_to(s_hand_path_ptr, s_hand_shift);
  gpath_draw_outline(ctx, s_hand_path_ptr);
}

static void draw_division(Layer * const layer, GContext * const ctx, 
  const int minute, const bool filled) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  const float angle_ = minute * 2 * PI / 60;
  const int x = (int)(  HandLength * sin(angle_)) + s_hand_shift.x;
  const int y = (int)(- HandLength * cos(angle_)) + s_hand_shift.y;
  const int radius = (minute % 5 == 0) ? 5 : 2;
  if (filled)
    graphics_fill_circle(ctx, GPoint(x, y), radius);
  else
    graphics_draw_circle(ctx, GPoint(x, y), radius);
}

static void draw_hand_filled(Layer * const layer, GContext * const ctx, const int second) {
  const int len = second * HandLength / 60;
  s_second_path_ptr->points[0].y = -len;
  s_second_path_ptr->points[1].y = -len;
  graphics_context_set_fill_color(ctx, GColorBlack);
  gpath_rotate_to(s_second_path_ptr, s_hand_angle);
  gpath_move_to(s_second_path_ptr, s_hand_shift);
  gpath_draw_filled(ctx, s_second_path_ptr);
}

static void draw_divisions(Layer * const layer, GContext * const ctx,
    const int hour24, const int minute) {
  const int hour12 = hour24 % 12;
  const bool is_am = hour24 < 12;
  if (is_am) {
    for (int i = minute - 12; i < minute - hour12; ++ i)
      draw_division(layer, ctx, (i + 60) % 60, false);
    for (int i = minute - hour12; i < minute; ++ i) 
      draw_division(layer, ctx, (i + 60) % 60, true);
    draw_division(layer, ctx, minute, true);
    for (int i = minute + 1; i <= minute + 12; ++ i)
      draw_division(layer, ctx, i % 60, false);
  } else {
    for (int i = minute - 12; i < minute; ++ i)
      draw_division(layer, ctx, i >= 0 ? i : i + 60, false);
    draw_division(layer, ctx, minute, true);
    for (int i = minute + 1; i <= minute + hour12; ++ i)
      draw_division(layer, ctx, i % 60,true);
    for (int i = minute + hour12 + 1; i <= minute + 12; ++ i)
      draw_division(layer, ctx, i % 60, false);
 }
}

static void update_main_layer(Layer *layer, GContext *ctx) {
  static WatchTime last_time = {
    .hour = -1, .minute = -1, .second = -1
  };
//  APP_LOG(APP_LOG_LEVEL_INFO, "update_main_layer()");

  if ((last_time.minute != s_current_time.minute) ||
      (last_time.hour != s_current_time.hour)) {
    prepare_drawing_hand_division(layer, ctx, 
      s_current_time.hour, s_current_time.minute);
    draw_hand_outline(layer, ctx);
    draw_hand_filled(layer, ctx, s_current_time.second);
    draw_divisions(layer, ctx, s_current_time.hour, s_current_time.minute);
  } else if (last_time.second != s_current_time.second) {
    draw_hand_filled(layer, ctx, s_current_time.second);
  }
  last_time = s_current_time;
}

static void create_rect_pathes(void) {
  s_hand_path_ptr = gpath_create(&HandPathInfo);
  s_second_path_ptr = gpath_create(&SecondPathInfo);
}

static void destroy_rect_pathes(void) {
  gpath_destroy(s_hand_path_ptr);  
  gpath_destroy(s_second_path_ptr);  
}

static void main_window_load(Window *window) {
//  APP_LOG(APP_LOG_LEVEL_INFO, "main_window_load()");
  s_main_layer = window_get_root_layer(s_main_window);
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

// main

static void init(void) {
  create_rect_pathes();
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void deinit(void) {
  destroy_rect_pathes();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
