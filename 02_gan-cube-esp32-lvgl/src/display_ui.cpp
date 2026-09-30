/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/get-started/platforms/arduino.html  */

#include <lvgl.h>
#include <Wire.h>
#include "esp_lcd_touch_axs5106l.h"

/*To use the built-in examples and demos of LVGL uncomment the includes below respectively.
 *You also need to copy `lvgl/examples` to `lvgl/src/examples`. Similarly for the demos `lvgl/demos` to `lvgl/src/demos`.
 Note that the `lv_examples` library is for LVGL v7 and you shouldn't install it for this version (since LVGL v8)
 as the examples and demos are now part of the main LVGL library. */

// #include <examples/lv_examples.h>
#include <demos/lv_demos.h>
#include <ArduinoJson.h>

// #define DIRECT_RENDER_MODE // Uncomment to enable full frame buffer

#include <Arduino_GFX_Library.h>
#include "solve_timer.h"
#include "gan_ble.h"
#include "mqtt_publish.h"

#define ROTATION 1
// #define ROTATION 1
// #define ROTATION 2
// #define ROTATION 3

#define GFX_BL 23
#define LEDC_FREQ 5000
#define LEDC_RESOLUTION 10

#define Touch_I2C_SDA 18
#define Touch_I2C_SCL 19
#define Touch_RST     20
#define Touch_INT     21
#define BATTERY_ADC_PIN 0


Arduino_DataBus *bus = new Arduino_HWSPI(15 /* DC */, 14 /* CS */, 1 /* SCK */, 2 /* MOSI */);

Arduino_GFX *gfx = new Arduino_ST7789(
  bus, 22 /* RST */, 0 /* rotation */, false /* IPS */,
  172 /* width */, 320 /* height */,
  34 /*col_offset1*/, 0 /*uint8_t row_offset1*/,
  34 /*col_offset2*/, 0 /*row_offset2*/);

void lcd_reg_init(void) {
  static const uint8_t init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x11,  // 2: Out of sleep mode, no args, w/delay
    END_WRITE,
    DELAY, 120,

    BEGIN_WRITE,
    WRITE_C8_D16, 0xDF, 0x98, 0x53,
    WRITE_C8_D8, 0xB2, 0x23, 

    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 4,
    0x00, 0x47, 0x00, 0x6F,

    WRITE_COMMAND_8, 0xBB,
    WRITE_BYTES, 6,
    0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,

    WRITE_C8_D16, 0xC0, 0x44, 0xA4,
    WRITE_C8_D8, 0xC1, 0x16, 

    WRITE_COMMAND_8, 0xC3,
    WRITE_BYTES, 8,
    0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,

    WRITE_COMMAND_8, 0xC4,
    WRITE_BYTES, 12,
    0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,

    WRITE_COMMAND_8, 0xC8,
    WRITE_BYTES, 32,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00, 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,

    WRITE_COMMAND_8, 0xD0,
    WRITE_BYTES, 5,
    0x04, 0x06, 0x6B, 0x0F, 0x00,

    WRITE_C8_D16, 0xD7, 0x00, 0x30,
    WRITE_C8_D8, 0xE6, 0x14, 
    WRITE_C8_D8, 0xDE, 0x01, 

    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 5,
    0x03, 0x13, 0xEF, 0x35, 0x35,

    WRITE_COMMAND_8, 0xC1,
    WRITE_BYTES, 3,
    0x14, 0x15, 0xC0,

    WRITE_C8_D16, 0xC2, 0x06, 0x3A,
    WRITE_C8_D16, 0xC4, 0x72, 0x12,
    WRITE_C8_D8, 0xBE, 0x00, 
    WRITE_C8_D8, 0xDE, 0x02, 

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x00, 0x02, 0x00,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x01, 0x02, 0x00,

    WRITE_C8_D8, 0xDE, 0x00, 
    WRITE_C8_D8, 0x35, 0x00, 
    WRITE_C8_D8, 0x3A, 0x05, 

    WRITE_COMMAND_8, 0x2A,
    WRITE_BYTES, 4,
    0x00, 0x22, 0x00, 0xCD,

    WRITE_COMMAND_8, 0x2B,
    WRITE_BYTES, 4,
    0x00, 0x00, 0x01, 0x3F,

    WRITE_C8_D8, 0xDE, 0x02, 

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x00, 0x02, 0x00,
    
    WRITE_C8_D8, 0xDE, 0x00, 
    WRITE_C8_D8, 0x36, 0x00,
    WRITE_COMMAND_8, 0x21,
    END_WRITE,
    
    DELAY, 10,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,  // 5: Main screen turn on, no args, w/delay
    END_WRITE
  };
  bus->batchOperation(init_operations, sizeof(init_operations));
}

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_disp_draw_buf_t draw_buf;
lv_color_t *disp_draw_buf;
lv_disp_drv_t disp_drv;

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf)
{
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
#ifndef DIRECT_RENDER_MODE
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
#endif // #ifndef DIRECT_RENDER_MODE

  lv_disp_flush_ready(disp_drv);
}

/*Read the touchpad*/
void touchpad_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
  touch_data_t touch_data;
  // uint16_t touchpad_x[1] = { 0 };
  // uint16_t touchpad_y[1] = { 0 };
  uint8_t touchpad_cnt = 0;

  /* Read touch controller data */
  // esp_lcd_touch_read_data(touch_handle);
  bsp_touch_read();
  /* Get coordinates */
  // bool touchpad_pressed = esp_lcd_touch_get_coordinates(touch_handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);
  bool touchpad_pressed = bsp_touch_get_coordinates(&touch_data);

  if (touchpad_pressed) {
    data->point.x = touch_data.coords[0].x;
    data->point.y = touch_data.coords[0].y;
    data->state = LV_INDEV_STATE_PRESSED;
    // printf("x:%03d, y:%03d\r\n", touchpad_x[0], touchpad_y[0]);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static lv_obj_t* timeLabel;
static lv_obj_t* bleIndicator;
static lv_obj_t* networkIndicator;
static lv_obj_t* batteryLabel;
static lv_obj_t* resultPanel;
static lv_obj_t* resultTimeLabel;
static lv_obj_t* resultLabels[3];
static lv_obj_t* startButton;
static lv_obj_t* startButtonLabel;
static bool waitingForStart = true;
static uint32_t lastBatteryUpdateMs = 0;

static const lv_color_t CYBER_BG = lv_color_hex(0x070B16);
static const lv_color_t CYBER_PANEL = lv_color_hex(0x10182A);
static const lv_color_t CYBER_CYAN = lv_color_hex(0x36E7FF);
static const lv_color_t CYBER_MAGENTA = lv_color_hex(0xFF3CAC);
static const lv_color_t CYBER_LIME = lv_color_hex(0xB7FF4A);
static const lv_color_t CYBER_TEXT = lv_color_hex(0xE8F7FF);

static void driftX(void* object, int32_t value) {
  lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
}

static void driftY(void* object, int32_t value) {
  lv_obj_set_y(static_cast<lv_obj_t*>(object), value);
}

static void animateCyberObject(lv_obj_t* object, lv_coord_t start, lv_coord_t end,
                               uint32_t duration, bool horizontal) {
  lv_anim_t animation;
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, object);
  lv_anim_set_values(&animation, start, end);
  lv_anim_set_time(&animation, duration);
  lv_anim_set_playback_time(&animation, duration);
  lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&animation, horizontal ? driftX : driftY);
  lv_anim_start(&animation);
}

static lv_obj_t* cyberPanel(lv_obj_t* parent, lv_coord_t width, lv_coord_t height,
                            lv_color_t color = CYBER_PANEL) {
  lv_obj_t* panel = lv_obj_create(parent);
  lv_obj_remove_style_all(panel);
  lv_obj_set_size(panel, width, height);
  lv_obj_set_style_bg_color(panel, color, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x24506A), LV_PART_MAIN);
  lv_obj_set_style_radius(panel, 10, LV_PART_MAIN);
  return panel;
}

static lv_obj_t* cyberText(lv_obj_t* parent, const char* text, const lv_font_t* font,
                           lv_color_t color, lv_align_t align, lv_coord_t x, lv_coord_t y) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
  lv_obj_set_style_text_letter_space(label, 1, LV_PART_MAIN);
  lv_obj_align(label, align, x, y);
  return label;
}

static void updateBatteryIndicator() {
  if (batteryLabel == nullptr) return;

  const uint32_t batteryVoltageMv = analogReadMilliVolts(BATTERY_ADC_PIN) * 3UL;
  int batteryPercent = map(batteryVoltageMv, 3200, 4200, 0, 100);
  batteryPercent = constrain(batteryPercent, 0, 100);
  lv_label_set_text_fmt(batteryLabel, "%d%%", batteryPercent);
}

static void brightnessSliderEvent(lv_event_t* event)
{
  lv_obj_t* slider = lv_event_get_target(event);
  const int value = lv_slider_get_value(slider);
  ledcWrite(GFX_BL, (1 << LEDC_RESOLUTION) * value / 100);
}

static void startButtonEvent(lv_event_t* event) {
  LV_UNUSED(event);
  solveTimerReset();
  waitingForStart = false;
  lv_obj_set_style_bg_color(startButton, lv_color_hex(0x24324A), LV_PART_MAIN);
  lv_obj_set_style_bg_color(startButton, lv_color_hex(0x24324A), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_color(startButton, CYBER_LIME, LV_PART_MAIN);
  lv_obj_set_style_border_color(startButton, CYBER_LIME, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_label_set_text(startButtonLabel, "RESET");
}

static void resultBackButtonEvent(lv_event_t* event) {
  LV_UNUSED(event);
  lv_obj_add_flag(resultPanel, LV_OBJ_FLAG_HIDDEN);
}

static void createTimerScreen()
{
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, CYBER_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

  // Dedicated bottom layer: animated decoration can never cover the HUD.
  lv_obj_t* backgroundLayer = lv_obj_create(screen);
  lv_obj_remove_style_all(backgroundLayer);
  lv_obj_set_size(backgroundLayer, 320, 172);
  lv_obj_set_pos(backgroundLayer, 0, 0);
  lv_obj_move_background(backgroundLayer);

  // Subtle cyber-grid: lightweight LVGL objects, no bitmap asset required.
  for (int y = 34; y < 172; y += 20) {
    lv_obj_t* grid = lv_obj_create(backgroundLayer);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 320, 1);
    lv_obj_set_pos(grid, 0, y);
    lv_obj_set_style_bg_color(grid, lv_color_hex(0x14243A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(grid, LV_OPA_50, LV_PART_MAIN);
  }

  // Slow ambient motion keeps the HUD alive without distracting from the timer.
  const lv_coord_t circleX[] = {18, 82, 146, 226, 286, 48, 188};
  const lv_coord_t circleY[] = {52, 96, 36, 116, 62, 142, 82};
  const lv_coord_t circleSize[] = {18, 10, 26, 14, 22, 8, 16};
  for (uint8_t index = 0; index < 7; ++index) {
    lv_obj_t* circle = lv_obj_create(backgroundLayer);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, circleSize[index], circleSize[index]);
    lv_obj_set_pos(circle, circleX[index], circleY[index]);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(circle, index == 1 ? CYBER_MAGENTA : CYBER_CYAN, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(circle, index % 3 == 1 ? LV_OPA_60 : LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_border_width(circle, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(circle, index == 1 ? CYBER_MAGENTA : CYBER_CYAN, LV_PART_MAIN);
    lv_obj_set_style_border_opa(circle, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(circle, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(circle, index == 1 ? CYBER_MAGENTA : CYBER_CYAN, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(circle, LV_OPA_50, LV_PART_MAIN);
    animateCyberObject(circle, circleX[index] - 7, circleX[index] + 7,
                       4200 + index * 450, true);
    animateCyberObject(circle, circleY[index] - 4, circleY[index] + 8,
                       5200 + index * 380, false);
  }
  for (uint8_t index = 0; index < 2; ++index) {
    lv_obj_t* scanLine = lv_obj_create(backgroundLayer);
    lv_obj_remove_style_all(scanLine);
    lv_obj_set_size(scanLine, 80, 1);
    lv_obj_set_pos(scanLine, index == 0 ? -80 : 320, 72 + index * 54);
    lv_obj_set_style_bg_color(scanLine, index == 0 ? CYBER_MAGENTA : CYBER_CYAN, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scanLine, LV_OPA_50, LV_PART_MAIN);
    animateCyberObject(scanLine, index == 0 ? -80 : 320,
                       index == 0 ? 320 : -80, 6200 + index * 900, true);
  }
  cyberText(screen, "LIVE SOLVE SYSTEM", &lv_font_montserrat_12, CYBER_CYAN,
            LV_ALIGN_TOP_LEFT, 10, 8);

  lv_obj_t* headerLine = lv_obj_create(screen);
  lv_obj_remove_style_all(headerLine);
  lv_obj_set_size(headerLine, 94, 2);
  lv_obj_align(headerLine, LV_ALIGN_TOP_LEFT, 10, 43);
  lv_obj_set_style_bg_color(headerLine, CYBER_MAGENTA, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(headerLine, LV_OPA_COVER, LV_PART_MAIN);

  bleIndicator = lv_obj_create(screen);
  networkIndicator = lv_obj_create(screen);
  for (lv_obj_t* indicator : {bleIndicator, networkIndicator}) {
    lv_obj_remove_style_all(indicator);
    lv_obj_set_size(indicator, 12, 12);
    lv_obj_set_style_radius(indicator, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  }
  lv_obj_align(bleIndicator, LV_ALIGN_TOP_RIGHT, -34, 10);
  lv_obj_align(networkIndicator, LV_ALIGN_TOP_RIGHT, -52, 10);

  batteryLabel = cyberText(screen, "--%", &lv_font_montserrat_12, CYBER_TEXT,
                           LV_ALIGN_TOP_RIGHT, -34, 28);
  lv_label_set_text(batteryLabel, "--%");

  lv_obj_t* brightnessSlider = lv_slider_create(screen);
  lv_slider_set_range(brightnessSlider, 1, 100);
  lv_slider_set_value(brightnessSlider, 10, LV_ANIM_OFF);
  lv_obj_set_size(brightnessSlider, 8, 58);
  lv_obj_align(brightnessSlider, LV_ALIGN_TOP_RIGHT, -12, 62);
  lv_obj_set_style_bg_color(brightnessSlider, lv_color_hex(0x1A2A42), LV_PART_MAIN);
  lv_obj_set_style_bg_color(brightnessSlider, CYBER_MAGENTA, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightnessSlider, CYBER_TEXT, LV_PART_KNOB);
  lv_obj_add_event_cb(brightnessSlider, brightnessSliderEvent, LV_EVENT_VALUE_CHANGED, NULL);

  timeLabel = lv_label_create(screen);
  lv_label_set_text(timeLabel, "READY");
  lv_obj_set_style_text_color(timeLabel, CYBER_TEXT, LV_PART_MAIN);
  lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_letter_space(timeLabel, 2, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(timeLabel, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_color(timeLabel, lv_color_hex(0x06b6d4), LV_PART_MAIN);
  lv_obj_set_style_shadow_opa(timeLabel, LV_OPA_60, LV_PART_MAIN);
  lv_obj_align(timeLabel, LV_ALIGN_CENTER, -8, -12);

  lv_obj_t* timerPanel = cyberPanel(screen, 156, 52);
  lv_obj_align(timerPanel, LV_ALIGN_CENTER, -8, -12);
  lv_obj_set_style_border_color(timerPanel, lv_color_hex(0x1D7E9B), LV_PART_MAIN);
  lv_obj_set_style_shadow_width(timerPanel, 18, LV_PART_MAIN);
  lv_obj_set_style_shadow_color(timerPanel, lv_color_hex(0x073B55), LV_PART_MAIN);
  lv_obj_set_style_shadow_opa(timerPanel, LV_OPA_60, LV_PART_MAIN);
  lv_obj_move_foreground(timeLabel);

  startButton = lv_btn_create(screen);
  lv_obj_set_size(startButton, 146, 42);
  lv_obj_set_style_radius(startButton, 10, LV_PART_MAIN);
  lv_obj_set_style_bg_color(startButton, lv_color_hex(0x18243B), LV_PART_MAIN);
  lv_obj_set_style_bg_color(startButton, lv_color_hex(0x2B1640), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(startButton, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(startButton, CYBER_MAGENTA, LV_PART_MAIN);
  lv_obj_set_style_border_color(startButton, CYBER_CYAN, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(startButton, 18, LV_PART_MAIN);
  lv_obj_set_style_shadow_color(startButton, CYBER_MAGENTA, LV_PART_MAIN);
  lv_obj_set_style_shadow_opa(startButton, LV_OPA_50, LV_PART_MAIN);
  lv_obj_align(startButton, LV_ALIGN_CENTER, -8, 52);
  lv_obj_add_event_cb(startButton, startButtonEvent, LV_EVENT_CLICKED, NULL);

  startButtonLabel = lv_label_create(startButton);
  lv_label_set_text(startButtonLabel, "START");
  lv_obj_set_style_text_color(startButtonLabel, CYBER_TEXT, LV_PART_MAIN);
  lv_obj_set_style_text_font(startButtonLabel, &lv_font_unscii_16, LV_PART_MAIN);
  lv_obj_center(startButtonLabel);

  resultPanel = lv_obj_create(screen);
  lv_obj_set_size(resultPanel, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(resultPanel, CYBER_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(resultPanel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_add_flag(resultPanel, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* resultTitle = lv_label_create(resultPanel);
  lv_label_set_text(resultTitle, "ERGEBNIS");
  lv_obj_set_style_text_color(resultTitle, CYBER_MAGENTA, LV_PART_MAIN);
  lv_obj_set_style_text_font(resultTitle, &lv_font_unscii_16, LV_PART_MAIN);
  lv_obj_align(resultTitle, LV_ALIGN_TOP_MID, 0, 58);

  resultTimeLabel = lv_label_create(resultPanel);
  lv_label_set_text(resultTimeLabel, "00:00.00");
  lv_obj_set_style_text_color(resultTimeLabel, CYBER_CYAN, LV_PART_MAIN);
  lv_obj_set_style_text_font(resultTimeLabel, &lv_font_unscii_16, LV_PART_MAIN);
  lv_obj_align(resultTimeLabel, LV_ALIGN_TOP_MID, 0, 18);

  for (uint8_t index = 0; index < 3; ++index) {
    resultLabels[index] = lv_label_create(resultPanel);
    lv_obj_set_style_text_color(resultLabels[index], CYBER_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(resultLabels[index], &lv_font_unscii_16, LV_PART_MAIN);
    lv_obj_align(resultLabels[index], LV_ALIGN_TOP_LEFT, 24, 100 + index * 38);
    lv_label_set_text(resultLabels[index], "-");
  }

  lv_obj_t* backButton = lv_btn_create(resultPanel);
  lv_obj_set_size(backButton, 100, 42);
  lv_obj_align(backButton, LV_ALIGN_TOP_MID, 0, 220);
  lv_obj_add_event_cb(backButton, resultBackButtonEvent, LV_EVENT_CLICKED, NULL);
  lv_obj_t* backLabel = lv_label_create(backButton);
  lv_label_set_text(backLabel, "BACK");
  lv_obj_center(backLabel);
}

void displayUiSetCurrentSolve(uint32_t timeMs) {
  if (resultTimeLabel == nullptr) return;
  lv_label_set_text_fmt(resultTimeLabel, "%02lu:%02lu.%02lu",
                        timeMs / 60000UL, (timeMs / 1000UL) % 60UL,
                        (timeMs / 10UL) % 100UL);
}

void displayUiSolveFinished() {
  if (startButton == nullptr || startButtonLabel == nullptr) return;
  lv_obj_set_style_bg_color(startButton, lv_color_hex(0x18243B), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(startButton, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(startButton, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(startButton, CYBER_MAGENTA, LV_PART_MAIN);
  lv_label_set_text(startButtonLabel, "START");
}

void displayUiShowResults(const uint8_t* payload, size_t length) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok || !doc.is<JsonArray>()) return;
  JsonArray results = doc.as<JsonArray>();
  for (uint8_t index = 0; index < 3; ++index) {
    if (index >= results.size()) {
      lv_label_set_text(resultLabels[index], "-");
      continue;
    }
    const uint32_t timeMs = results[index]["solvingtime"] | 0;
    const uint32_t moves = results[index]["moves"] | 0;
    lv_label_set_text_fmt(resultLabels[index], "%u.  %02lu:%02lu.%02lu   %lu",
                          index + 1, timeMs / 60000UL, (timeMs / 1000UL) % 60UL,
                          (timeMs / 10UL) % 100UL, static_cast<unsigned long>(moves));
  }
  lv_obj_clear_flag(resultPanel, LV_OBJ_FLAG_HIDDEN);
}

static void updateConnectionIndicator()
{
  lv_obj_set_style_bg_color(bleIndicator,
                            lv_color_hex(ganBleIsConnected() ? 0x36E7FF : 0x26344A),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(networkIndicator,
                            lv_color_hex(mqttPublishWifiConnected() && mqttPublishMqttConnected()
                                             ? 0xB7FF4A : 0x26344A),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bleIndicator, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(networkIndicator, LV_OPA_COVER, LV_PART_MAIN);
}

void displayUiSetup()
{
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  Serial.println("Arduino_GFX LVGL_Arduino_v8 example ");
  String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  // Init Display
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }
  lcd_reg_init();
  gfx->setRotation(ROTATION);
  gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
  ledcAttach(GFX_BL, LEDC_FREQ, LEDC_RESOLUTION);
  ledcWrite(GFX_BL, (1 << LEDC_RESOLUTION) * 10 / 100);
#endif

  // Init touch device
  //touch_init(gfx->width(), gfx->height(), gfx->getRotation());
  Wire.begin(Touch_I2C_SDA, Touch_I2C_SCL);
  bsp_touch_init(&Wire, Touch_RST, Touch_INT, gfx->getRotation(), gfx->width(), gfx->height());
  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  screenWidth = gfx->width();
  screenHeight = gfx->height();

#ifdef DIRECT_RENDER_MODE
  bufSize = screenWidth * screenHeight;
#else
  bufSize = screenWidth * 40;
#endif

#ifdef ESP32
#if defined(DIRECT_RENDER_MODE) && (defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL))
  disp_draw_buf = (lv_color_t *)gfx->getFramebuffer();
#else  // !(defined(DIRECT_RENDER_MODE) && (defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL)))
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!disp_draw_buf)
  {
    // remove MALLOC_CAP_INTERNAL flag try again
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
  }
#endif // !(defined(DIRECT_RENDER_MODE) && (defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL)))
#else // !ESP32
  Serial.println("LVGL disp_draw_buf heap_caps_malloc failed! malloc again...");
  disp_draw_buf = (lv_color_t *)malloc(bufSize * 2);
#endif // !ESP32
  if (!disp_draw_buf)
  {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  }
  else
  {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, bufSize);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
#ifdef DIRECT_RENDER_MODE
    disp_drv.direct_mode = true;
#endif
    lv_disp_drv_register(&disp_drv);

    /* Initialize the (dummy) input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read_cb;
    lv_indev_drv_register(&indev_drv);
    
    /* Option 2: Try an example. See all the examples
     * online: https://docs.lvgl.io/master/examples.html
     * source codes: https://github.com/lvgl/lvgl/tree/e7f88efa5853128bf871dde335c0ca8da9eb7731/examples */
    // lv_example_btn_1();

    /* Option 3: Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMOS_WIDGETS*/
    createTimerScreen();
    // lv_demo_benchmark();
    // lv_demo_keypad_encoder();
    // lv_demo_music();
    // lv_demo_stress();
  }

  Serial.println("Setup done");
}

void displayUiLoop()
{
  updateConnectionIndicator();
  if (millis() - lastBatteryUpdateMs >= 30000 || lastBatteryUpdateMs == 0) {
    updateBatteryIndicator();
    lastBatteryUpdateMs = millis();
  }
  lv_timer_handler(); /* let the GUI do its work */

  if (!waitingForStart) {
    const uint32_t elapsed = solveTimerElapsedMs();
    lv_label_set_text_fmt(timeLabel, "%02lu:%02lu.%02lu",
                          elapsed / 60000UL,
                          (elapsed / 1000UL) % 60UL,
                          (elapsed / 10UL) % 100UL);
  }

#ifdef DIRECT_RENDER_MODE
#if defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL)
  gfx->flush();
#else // !(defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL))
#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
#else
  gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
#endif
#endif // !(defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL))
#else  // !DIRECT_RENDER_MODE
#ifdef CANVAS
  gfx->flush();
#endif
#endif // !DIRECT_RENDER_MODE

  delay(5);
}
