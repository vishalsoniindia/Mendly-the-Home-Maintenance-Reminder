/*
   Esp32 Board : 2.0.10
   TFT_eSPI : 2.5.43
   lvgl : 8.3.10
   ESP32Time : 2.0.6

*/


/*Using LVGL with Arduino requires some extra steps:
  Be sure to read the docs here: https://docs.lvgl.io/master/get-started/platforms/arduino.html  */

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "lv_conf.h"
#include <demos/lv_demos.h>
#include "CST816S.h"
#include "ui.h"

#include <ESP32Time.h>  //{2.0.6}  [https://github.com/fbiego/ESP32Time]
ESP32Time rtc;

struct tm currentTime;

struct tm startTimeAC;
struct tm startTimeRO;
struct tm startTimeVC;
struct tm startTimeEX;

struct tm targetTimeAC;
struct tm targetTimeRO;
struct tm targetTimeVC;
struct tm targetTimeEX;


/*To use the built-in examples and demos of LVGL uncomment the includes below respectively.
  You also need to copy `lvgl/examples` to `lvgl/src/examples`. Similarly for the demos `lvgl/demos` to `lvgl/src/demos`.
  Note that the `lv_examples` library is for LVGL v7 and you shouldn't install it for this version (since LVGL v8)
  as the examples and demos are now part of the main LVGL library. */

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2

/*Change to your screen resolution*/
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 10 ];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */
CST816S touch(6, 7, 13, 5);	// sda, scl, rst, irq

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf)
{
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
  uint32_t w = ( area->x2 - area->x1 + 1 );
  uint32_t h = ( area->y2 - area->y1 + 1 );

  tft.startWrite();
  tft.setAddrWindow( area->x1, area->y1, w, h );
  tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
  tft.endWrite();

  lv_disp_flush_ready( disp_drv );
}

void example_increase_lvgl_tick(void *arg)
{
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static uint8_t count = 0;
void example_increase_reboot(void *arg)
{
  count++;
  if (count == 30) {
    // esp_restart();
  }

}

/*Read the touchpad*/
void my_touchpad_read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data )
{
  // uint16_t touchX, touchY;

  bool touched = touch.available();
  // touch.read_touch();
  if ( !touched )
    // if( 0!=touch.data.points )
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    data->point.x = touch.data.x;
    data->point.y = touch.data.y;
    // Serial.print( "Data x " );
    // Serial.println( touch.data.x );

    // Serial.print( "Data y " );
    // Serial.println( touch.data.y );
  }
}


// Function to calculate target time by adding months
struct tm calculateTargetTime(struct tm currentTime, int monthsToAdd) {
  struct tm target = currentTime;

  // Add months
  target.tm_mon += monthsToAdd;

  // Handle year rollover
  while (target.tm_mon > 11) { // tm_mon ranges from 0-11
    target.tm_mon -= 12;
    target.tm_year++;
  }

  // Ensure day is valid for the target month
  int daysInMonth = getDaysInMonth(target.tm_mon + 1, target.tm_year + 1900);
  if (target.tm_mday > daysInMonth) {
    target.tm_mday = daysInMonth;
  }

  return target;
}

// Function to get number of days in a month
int getDaysInMonth(int month, int year) {
  if (month == 2) {
    // February - check for leap year
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  } else if (month == 4 || month == 6 || month == 9 || month == 11) {
    return 30;
  } else {
    return 31;
  }
}

// Function to compare two tm structures (returns true if time1 >= time2)
bool isTimeReached(struct tm time1, struct tm time2) {
  time_t t1 = mktime(&time1);
  time_t t2 = mktime(&time2);
  return t1 >= t2;
}

// Function to print date and time
void printDateTime(struct tm timeinfo) {
  Serial.print(timeinfo.tm_year + 1900);
  Serial.print('-');
  if (timeinfo.tm_mon + 1 < 10) Serial.print('0');
  Serial.print(timeinfo.tm_mon + 1);
  Serial.print('-');
  if (timeinfo.tm_mday < 10) Serial.print('0');
  Serial.print(timeinfo.tm_mday);
  Serial.print(' ');
  if (timeinfo.tm_hour < 10) Serial.print('0');
  Serial.print(timeinfo.tm_hour);
  Serial.print(':');
  if (timeinfo.tm_min < 10) Serial.print('0');
  Serial.print(timeinfo.tm_min);
  Serial.print(':');
  if (timeinfo.tm_sec < 10) Serial.print('0');
  Serial.print(timeinfo.tm_sec);
  Serial.println();
}

float calculatePercentageComplete(struct tm current, struct tm start, struct tm target) {
  time_t start_t = mktime(&start);
  time_t current_t = mktime(&current);
  time_t target_t = mktime(&target);

  // Calculate total duration and elapsed time in seconds
  time_t totalDuration = target_t - start_t;
  time_t elapsedTime = current_t - start_t;

  // Ensure we don't divide by zero and handle edge cases
  if (totalDuration <= 0) return 100.0;
  if (elapsedTime <= 0) return 0.0;
  if (elapsedTime >= totalDuration) return 100.0;

  // Calculate percentage
  float percentage = (float(elapsedTime) / float(totalDuration)) * 100.0;

  // Clamp between 0 and 100
  return constrain(percentage, 0.0, 100.0);
}

#define GFX_BL 2 // default backlight pin, you may replace DF_GFX_BL to actual backlight pin
void setup()
{
  Serial.begin( 115200 ); /* prepare for possible serial debug */
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  Serial.println( LVGL_Arduino );
  Serial.println( "I am LVGL_Arduino" );

  lv_init();
#if LV_USE_LOG != 0
  lv_log_register_print_cb( my_print ); /* register print function for debugging */
#endif

  tft.begin();          /* TFT init */
  tft.setRotation( 0 ); /* Landscape orientation, flipped */

  /*Set the touchscreen calibration data,
    the actual data for your display can be acquired using
    the Generic -> Touch_calibrate example from the TFT_eSPI library*/
  // uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
  // tft.setTouch( calData );
  touch.begin();

  lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * screenHeight / 10 );

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register( &indev_drv );

  /* Create simple label */
  lv_obj_t *label = lv_label_create( lv_scr_act() );
  lv_label_set_text( label, "Hello Ardino and LVGL!");
  lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

  /* Try an example. See all the examples
     online: https://docs.lvgl.io/master/examples.html
     source codes: https://github.com/lvgl/lvgl/tree/e7f88efa5853128bf871dde335c0ca8da9eb7731/examples */
  //lv_example_btn_1();

  const esp_timer_create_args_t lvgl_tick_timer_args = {
    .callback = &example_increase_lvgl_tick,
    .name = "lvgl_tick"
  };

  const esp_timer_create_args_t reboot_timer_args = {
    .callback = &example_increase_reboot,
    .name = "reboot"
  };

  esp_timer_handle_t lvgl_tick_timer = NULL;
  esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

  esp_timer_handle_t reboot_timer = NULL;
  esp_timer_create(&reboot_timer_args, &reboot_timer);
  esp_timer_start_periodic(reboot_timer, 2000 * 1000);

  /*Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMOS_WIDGETS*/
  // lv_demo_widgets();
  // lv_demo_benchmark();
  // lv_demo_keypad_encoder();
  //    lv_demo_music();
  // lv_demo_printer();
  // lv_demo_stress();
  ui_init();

  Serial.print("TIME SETUP: ");
  printDateTime(rtc.getTimeStruct());

  Serial.println( "Setup done" );
  lv_scr_load(ui_bar);
}

int ac_month = 0;
int ro_month = 0;
int ex_month = 0;
int vc_month = 0;


void loop()
{
  lv_timer_handler(); /* let the GUI do its work */
  delay( 5 );

  currentTime = rtc.getTimeStruct();

  if (is_ac_month_changed) {
    is_ac_month_changed = false;

    ac_month = lv_dropdown_get_selected(ui_Dropdown1);
    Serial.print("AC Month:");
    Serial.println(ac_month);

    startTimeAC = currentTime;
    targetTimeAC = calculateTargetTime(currentTime, ac_month);
    targetTimeAC.tm_sec = targetTimeAC.tm_sec + 10;
    printDateTime(targetTimeAC);

  }

  if (is_ex_month_changed) {
    is_ex_month_changed = false;

    ex_month = lv_dropdown_get_selected(ui_Dropdown3);
    Serial.print("EX Month:");
    Serial.println(ex_month);

    startTimeEX = currentTime;
    targetTimeEX = calculateTargetTime(currentTime, ex_month);
  }

  if (is_vc_month_changed) {
    is_vc_month_changed = false;

    vc_month = lv_dropdown_get_selected(ui_Dropdown2);
    Serial.print("VC Month:");
    Serial.println(vc_month);

    startTimeVC = currentTime;
    targetTimeVC = calculateTargetTime(currentTime, vc_month);
  }

  if (is_ro_month_changed) {
    is_ro_month_changed = false;

    ro_month = lv_dropdown_get_selected(ui_Dropdown4);
    Serial.print("RO Month:");
    Serial.println(ro_month);

    startTimeRO = currentTime;
    targetTimeRO = calculateTargetTime(currentTime, ro_month);
  }

  if (isTimeReached(currentTime, targetTimeAC) && !is_ac_alert && ac_month) {
    is_ac_alert = true;
  }
  if (isTimeReached(currentTime, targetTimeRO) && !is_ro_alert && ro_month) {
    is_ro_alert = true;
  }
  if (isTimeReached(currentTime, targetTimeVC) && !is_vc_alert && vc_month) {
    is_vc_alert = true;
  }
  if (isTimeReached(currentTime, targetTimeEX) && !is_ex_alert && ex_month) {
    is_ex_alert = true;
  }

  if (is_ac_alert) {
    lv_scr_load(ui_acerror);
  }
  else if (is_ro_alert) {
    lv_scr_load(ui_roerror);
  }
  else if (is_vc_alert) {
    lv_scr_load(ui_vcerror);
  }
  else if (is_ex_alert) {
    lv_scr_load(ui_exerror);
  }

  int percentage = calculatePercentageComplete(currentTime, startTimeAC, targetTimeAC);
  lv_bar_set_value(ui_bar_ac, percentage , LV_ANIM_OFF);

  percentage = calculatePercentageComplete(currentTime, startTimeRO, targetTimeRO);
  lv_bar_set_value(ui_bar_ro, percentage , LV_ANIM_OFF);

  percentage = calculatePercentageComplete(currentTime, startTimeEX, targetTimeEX);
  lv_bar_set_value(ui_bar_ex, percentage , LV_ANIM_OFF);

  percentage = calculatePercentageComplete(currentTime, startTimeVC, targetTimeVC);
  lv_bar_set_value(ui_bar_vc, percentage , LV_ANIM_OFF);


  if (!ex_month) {
    lv_bar_set_value(ui_bar_ex, 0 , LV_ANIM_OFF);
  }
  if (!ac_month) {
    lv_bar_set_value(ui_bar_ac, 0 , LV_ANIM_OFF);
  }
  if (!ro_month) {
    lv_bar_set_value(ui_bar_ro, 0 , LV_ANIM_OFF);
  }
  if (!vc_month) {
    lv_bar_set_value(ui_bar_vc, 0 , LV_ANIM_OFF);
  }

  //    if(millis() - previous > 10000){
  //      previous = millis();
  //      if(val == 100){
  //        lv_scr_load(ui_roerror);
  //        val = 0;
  //      }
  //      lv_bar_set_value(ui_bar_ro, val , LV_ANIM_OFF);
  //      val = val + 10;
  //    }
}
