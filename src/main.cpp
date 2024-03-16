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
static lv_disp_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10]; /* Declare a buffer for 10 lines */


#define MOSI_PIN 32
#define MISO_PIN 39
#define CLK_PIN  25
#define CS_PIN   33
#define RERUN_CALIBRATE true
XPT2046_Bitbang touchscreen(MOSI_PIN, MISO_PIN, CLK_PIN, CS_PIN);

void event_handler(lv_obj_t* btn, lv_event_t event) {
    Serial.println("EVENT");
    if(event == LV_EVENT_CLICKED) { // button was clicked
        Serial.println("Button clicked");
    }
}


    /* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    Serial.println("Flushing display...");

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);

    // Directly send pixel data to the display
    for(int y = area->y1; y <= area->y2; y++) {
        for(int x = area->x1; x <= area->x2; x++) {
            // Calculate the position within the buffer
            uint32_t buffer_pos = (y - area->y1) * w + (x - area->x1);
            // Extract the color from LVGL's buffer and write to display
            uint16_t color = color_p[buffer_pos].full;
            tft.writeColor(color, 1);
        }
    }

    tft.endWrite();
    lv_disp_flush_ready(disp); // Inform LVGL that flushing is done

    Serial.println("Display flushed.");


}

bool my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
    Point touchPoint = touchscreen.getTouch(); // Poll the touch controller
    // Serial.print("touched point x: ");
    // Serial.println(touchPoint.x);
    // Serial.print("touched point y: ");
    // Serial.println(touchPoint.y);
    int16_t tmp_x = touchPoint.x;
    int16_t tmp_y = touchPoint.y;

    /* Coordinate rotation/modification to match the display orientation */
    touchPoint.x = 320 - tmp_y;  // Replace 320 with your display width
    touchPoint.y = tmp_x;   // No changes necessary on the Y axis


    if (touchPoint.x >= 0 && touchPoint.x < 240 && touchPoint.y >= 0 && touchPoint.y < 320) {  
        // if the touch coordinates are within the display's coordinates
        data->state = LV_INDEV_STATE_PR; 
        /*Set the coordinates*/
        data->point.x = touchPoint.x;
        data->point.y = touchPoint.y;
        // Serial.println("Valid touch point");
    } else {
        data->state = LV_INDEV_STATE_REL;  // if the touch screen is not touched (i.e., the touch coordinates are outside the display's coordinates)
        // Serial.println("Invalid touch point");

    }

    return false; /*Return `false` because we are not buffering and no more data to read*/
}


void setup() {
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

    lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);
    Serial.println("LVGL display driver registered.");

    lv_indev_drv_t indev_drv; //Descriptor of a touchpad/input device driver
    lv_indev_drv_init(&indev_drv); //Basic initialization
    indev_drv.type = LV_INDEV_TYPE_POINTER; // The touchpad is of type POINTER
    indev_drv.read_cb = my_touchpad_read; // Set your driver function
    lv_indev_drv_register(&indev_drv); // Finally register the touchpad driver
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


    // Create a style for the text
    // static lv_style_t style_text;
    // lv_style_init(&style_text);
    // // Set the text color. Example: RGB(255, 0, 0) for red
    // lv_style_set_text_color(&style_text, LV_STATE_DEFAULT, LV_COLOR_MAKE(255, 0, 0));


    // // Display the text from SD card
    // lv_obj_t *label = lv_label_create(lv_scr_act(), NULL);
    
    // lv_label_set_text(label, line.c_str());
    // lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);
    // Serial.println("Displayed text on screen.");


    lv_obj_t *btn = lv_btn_create(lv_scr_act(), NULL); // create a button on the current screen
    lv_obj_set_event_cb(btn, event_handler); // set an event callback function to the button
    lv_obj_align(btn, NULL, LV_ALIGN_CENTER, 0, 0); // align to center
    lv_obj_t *label = lv_label_create(btn, NULL); // create a label on the button
    lv_label_set_text(label, "Click Me"); // set the label text

}
int counter = 0;
unsigned long lastTick = 0;


void loop() {
    static uint32_t prev_ms = millis();


    static lv_obj_t* circle = nullptr; // Define static to retain the value across loop() calls
    Point touchPoint = touchscreen.getTouch(); // Poll the touch controller

    // Serial.print("touch: ");
    // Serial.print(touchPoint.x);
    // Serial.print(", ");
    // Serial.println(touchPoint.y);

    // Serial.println("touchscreen location: " + String(touchPoint.x) + ", " + String(touchPoint.y));
    //     // Read the file
    // File file = SD.open("/hello.txt");
    // if(!file) {
    //     Serial.println("Failed to open file for reading");
    //     return;
    // }
    // String line;
    // while(file.available()){
    //     line = file.readStringUntil('\n');
    //     break; // Just read the first line for simplicity
    // }
    // file.close();
    // Serial.println("Read from /hello.txt: " + line + " " + String(counter));

    


    lv_task_handler(); /* Handle LVGL tasks */
    // Serial.println("lv_task_handler"); // print to console every time lv_task_handler() runs

    uint32_t elapsed_ms = millis() - prev_ms;

    if(elapsed_ms >= LVGL_TICK_PERIOD_MS) {
        /* Increment LVGL's internal tick count */
        lv_tick_inc(elapsed_ms);

        /* Update record of when we last updated LVGL's tick count */
        prev_ms += elapsed_ms;
    }

    delay(5);
    counter +=1;
}