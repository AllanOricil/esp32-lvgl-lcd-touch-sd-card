#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <FS.h>
#include <SD.h>
#include "SPI.h"
#include "XPT2046_Bitbang.h"

#define SD_CS 5 // Adjust to your SD card CS pin

#define LVGL_TICK_PERIOD_MS 1

TFT_eSPI tft = TFT_eSPI(); /* TFT instance */
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

#define LVGL_BUFFER_PIXELS (LCD_WIDTH*LCD_HEIGHT/4)
#define LVGL_BUFFER_MALLOC_FLAGS (MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)

#define MOSI_PIN 32
#define MISO_PIN 39
#define CLK_PIN  25
#define CS_PIN   33
#define RERUN_CALIBRATE true
XPT2046_Bitbang touchscreen(MOSI_PIN, MISO_PIN, CLK_PIN, CS_PIN);

static void event_handler(lv_event_t *e) {
    Serial.println("EVENT");
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) { // button was clicked
        Serial.println("Button clicked");
    }
}


/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    // Serial.println("Flushing display...");

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);

    for(int y = area->y1; y <= area->y2; y++) {
        for(int x = area->x1; x <= area->x2; x++) {
            uint32_t buffer_pos = (y - area->y1) * w + (x - area->x1);
            uint16_t color = color_p[buffer_pos].full;
            tft.writeColor(color, 1);
        }
    }

    tft.endWrite();
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
    Point touchPoint = touchscreen.getTouch();
    int16_t tmp_x = touchPoint.x;
    int16_t tmp_y = touchPoint.y;

    // NOTE: swapping due to the rotation
    touchPoint.x = tmp_y;
    touchPoint.y = tmp_x;

    if (touchPoint.x >= 0 && touchPoint.x < 240 && touchPoint.y >= 0 && touchPoint.y < 320) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchPoint.x;
        data->point.y = touchPoint.y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void setup() {
    // Display and touchscreen initialization here ..
    // SD Card Initialization
    Serial.begin(115200);
    while(!Serial); // Wait for serial port to connect. Needed for native USB
    Serial.println("Starting setup...");

    pinMode(21, OUTPUT); // Set pin 21 as an output for the backlight
    digitalWrite(21, HIGH); // Turn on the backlight

    // Initialize the touchscreen
    touchscreen.begin();
    // Check for existing calibration data
    if (!touchscreen.loadCalibration()) {
        Serial.println("Failed to load calibration data from SPIFFS.");
    }
    // Check if we need to re-run calibration
    #if RERUN_CALIBRATE
        Serial.println("Re-running calibration as requested...");
        delay(2000); //wait for user to see serial
        touchscreen.calibrate();
        touchscreen.saveCalibration();
    #endif

    delay(1000);
    Serial.println("Backlight enabled.");

    lv_init();
    Serial.println("LVGL initialized.");

    tft.begin(); /* TFT init */
    
    Serial.println("TFT initialized.");
    tft.setRotation(2); /* Landscape orientation */
    Serial.println("TFT rotation set.");

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = (lv_disp_draw_buf_t *)malloc(sizeof(lv_disp_draw_buf_t));
    void *drawBuffer = heap_caps_malloc(sizeof(lv_color_t) * LVGL_BUFFER_PIXELS, LVGL_BUFFER_MALLOC_FLAGS);
    lv_disp_draw_buf_init(disp_drv.draw_buf, drawBuffer, NULL, LVGL_BUFFER_PIXELS);
    lv_disp_t * disp = lv_disp_drv_register(&disp_drv);
    Serial.println("LVGL display driver registered.");

    lv_obj_clean(lv_scr_act());

    lv_indev_drv_init(&indev_drv);
    indev_drv.disp = disp;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read; 
    lv_indev_t * touch_indev = lv_indev_drv_register(&indev_drv); // registering device driver
    Serial.println("Touch driver initialized and registered.");

    // SD Card Initialization
    if(!SD.begin(SD_CS)) {
        Serial.println("Card Mount Failed");
        return;
    }
    Serial.println("SD card initialized.");

    // Format SD card
    if(SD.exists("/hello.txt")) {
        SD.remove("/hello.txt");
        Serial.println("/hello.txt exists. Removing...");
    }

    // Create a new file
    File file = SD.open("/hello.txt", FILE_WRITE);
    if(!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.println("Hello, hi from SD card!");
    file.close();
    Serial.println("Written 'Hello, hi from SD card!' to /hello.txt");

    // Read the file
    file = SD.open("/hello.txt");
    if(!file) {
        Serial.println("Failed to open file for reading");
        return;
    }
    String line;
    while(file.available()){
        line = file.readStringUntil('\n');
        break; // Just read the first line for simplicity
    }
    file.close();
    Serial.println("Read from /hello.txt: " + line);

    // Create a button with a label
    static lv_style_t style_btn_blue; 
    lv_style_init(&style_btn_blue); 
    lv_style_set_bg_color(&style_btn_blue, lv_color_hex(0x0000FF)); // Set the blue col
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_center(btn);
    lv_obj_t *label = lv_label_create(btn); 
    lv_label_set_text(label, "Click Me"); 
    lv_obj_add_style(btn, &style_btn_blue, 0);
}

int counter = 0;

void loop() {    
    static uint32_t prev_ms = millis();

    File file = SD.open("/hello.txt");
    if(!file) {
        Serial.println("Failed to open file for reading");
        return;
    }
    String line;
    while(file.available()){
        line = file.readStringUntil('\n');
        break; // Just read the first line for simplicity
    }
    file.close();
    Serial.println("Read from /hello.txt: " + line + " " + String(counter));

    lv_task_handler();

    uint32_t elapsed_ms = millis() - prev_ms;

    if(elapsed_ms >= LVGL_TICK_PERIOD_MS) {
        /* Increment LVGL's internal tick count */
        lv_tick_inc(elapsed_ms);

        /* Update record of when we last updated LVGL's tick count */
        prev_ms += elapsed_ms;
    }
}
