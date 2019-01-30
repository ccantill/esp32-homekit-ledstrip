#include "esp32_digital_led_lib.h"

#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"

#ifndef __cplusplus
#define nullptr  NULL
#endif

#define HIGH 1
#define LOW 0
#define OUTPUT GPIO_MODE_OUTPUT
#define INPUT GPIO_MODE_INPUT

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#define min(a, b)  ((a) < (b) ? (a) : (b))
#define max(a, b)  ((a) > (b) ? (a) : (b))
#define floor(a)   ((int)(a))
#define ceil(a)    ((int)((int)(a) < (a) ? (a+1) : (a)))

strand_t STRANDS[] = { // Avoid using any of the strapping pins on the ESP32
 {.rmtChannel = 0, .gpioNum = 16, .ledType = LED_SK6812W_V1, .brightLimit = 32, .numPixels = 300,
  .pixels = nullptr, ._stateVars = nullptr}
};

int STRANDCNT = sizeof(STRANDS)/sizeof(STRANDS[0]);

void gpioSetup(int gpioNum, int gpioMode, int gpioVal) {
  gpio_num_t gpioNumNative = (gpio_num_t)(gpioNum);
  gpio_mode_t gpioModeNative = (gpio_mode_t)(gpioMode);
  gpio_pad_select_gpio(gpioNumNative);
  gpio_set_direction(gpioNumNative, gpioModeNative);
  gpio_set_level(gpioNumNative, gpioVal);
}

uint32_t IRAM_ATTR millis()
{
  return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void delay(uint32_t ms)
{
  if (ms > 0) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
  }
}

// Global variables
float led_hue = 0;              // hue is scaled 0 to 360
float led_saturation = 59;      // saturation is scaled 0 to 100
float led_brightness = 100;     // brightness is scaled 0 to 100
bool led_on = false;            // on is boolean on or off

void on_wifi_ready();

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            printf("STA start\n");
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            printf("WiFI ready\n");
            on_wifi_ready();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            printf("STA disconnected\n");
            esp_wifi_connect();
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_init() {
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// This section is modified by the addition of white so that it assumes
// fully saturated colors, and then scales with white to lower saturation.
//
// Next, scale appropriately the pure color by mixing with the white channel.
// Saturation is defined as "the ratio of colorfulness to brightness" so we will
// do this by a simple ratio wherein the color values are scaled down by (1-S)
// while the white LED is placed at S.
 
// This will maintain constant brightness because in HSI, R+B+G = I. Thus, 
// S*(R+B+G) = S*I. If we add to this (1-S)*I, where I is the total intensity,
// the sum intensity stays constant while the ratio of colorfulness to brightness
// goes down by S linearly relative to total Intensity, which is constant.

#include "math.h"
#define DEG_TO_RAD(X) (M_PI*(X)/180)

void hsi2rgbw(float H, float S, float I, pixelColor_t* rgbw) {
  int r, g, b, w;
  float cos_h, cos_1047_h;
  H = fmod(H,360); // cycle H around to 0-360 degrees
  H = 3.14159*H/(float)180; // Convert to radians.
  S /= 100;
  I /= 100;
  S = S>0?(S<1?S:1):0; // clamp S and I to interval [0,1]
  I = I>0?(I<1?I:1):0;
  
  if(H < 2.09439) {
    cos_h = cos(H);
    cos_1047_h = cos(1.047196667-H);
    r = S*255*I/3*(1+cos_h/cos_1047_h);
    g = S*255*I/3*(1+(1-cos_h/cos_1047_h));
    b = 0;
    w = 255*(1-S)*I;
  } else if(H < 4.188787) {
    H = H - 2.09439;
    cos_h = cos(H);
    cos_1047_h = cos(1.047196667-H);
    g = S*255*I/3*(1+cos_h/cos_1047_h);
    b = S*255*I/3*(1+(1-cos_h/cos_1047_h));
    r = 0;
    w = 255*(1-S)*I;
  } else {
    H = H - 4.188787;
    cos_h = cos(H);
    cos_1047_h = cos(1.047196667-H);
    b = S*255*I/3*(1+cos_h/cos_1047_h);
    r = S*255*I/3*(1+(1-cos_h/cos_1047_h));
    g = 0;
    w = 255*(1-S)*I;
  }
  
  rgbw->r=r;
  rgbw->g=g;
  rgbw->b=b;
  rgbw->w=w;
}

void led_string_set() {
  pixelColor_t color;
  if(led_on) {
    hsi2rgbw(led_hue, led_saturation, led_brightness, &color);\
  } else {
    color = pixelFromRGBW(0,0,0,0);
  }
  printf("Requested color h=%f, s=%f, b=%f\n", led_hue, led_saturation, led_brightness);
  printf("Color set to r=%d, g=%d, b=%d, w=%d\n", color.r, color.g, color.b, color.w);
  strand_t* pStrand = &STRANDS[0];
  for (uint16_t i = 0; i < pStrand->numPixels; i++) {
    pStrand->pixels[i] = color;
  }
  digitalLeds_updatePixels(pStrand);
}

void led_write(bool on) {
  strand_t* pStrand = &STRANDS[0];
  for (uint16_t i = 0; i < pStrand->numPixels; i++) {
    pStrand->pixels[i] = pixelFromRGBW(on?255:0, on?255:0, on?255:0, on?255:0);
  }
  digitalLeds_updatePixels(pStrand);
}

void led_identify_task(void *_args) {
  for (int i=0; i<3; i++) {
    for (int j=0; j<2; j++) {
      led_write(true);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      led_write(false);
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    vTaskDelay(250 / portTICK_PERIOD_MS);
  }

  led_write(led_on);

  vTaskDelete(NULL);
}

void led_identify(homekit_value_t _value) {
  printf("LED identify\n");
  xTaskCreate(led_identify_task, "LED identify", 512, NULL, 2, NULL);
}

homekit_value_t led_on_get() {
  return HOMEKIT_BOOL(led_on);
}

void led_on_set(homekit_value_t value) {
  if (value.format != homekit_format_bool) {
    // printf("Invalid on-value format: %d\n", value.format);
    return;
  }

  led_on = value.bool_value;
  led_string_set();
}

homekit_value_t led_brightness_get() {
  return HOMEKIT_INT(led_brightness);
}
void led_brightness_set(homekit_value_t value) {
  if (value.format != homekit_format_int) {
    // printf("Invalid brightness-value format: %d\n", value.format);
    return;
  }
  led_brightness = value.int_value;
  led_string_set();
}

homekit_value_t led_hue_get() {
  return HOMEKIT_FLOAT(led_hue);
}

void led_hue_set(homekit_value_t value) {
  if (value.format != homekit_format_float) {
    // printf("Invalid hue-value format: %d\n", value.format);
    return;
  }
  led_hue = value.float_value;
  led_string_set();
}

homekit_value_t led_saturation_get() {
  return HOMEKIT_FLOAT(led_saturation);
}

void led_saturation_set(homekit_value_t value) {
  if (value.format != homekit_format_float) {
    // printf("Invalid sat-value format: %d\n", value.format);
    return;
  }
  led_saturation = value.float_value;
  led_string_set();
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_lightbulb, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sample LED"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HaPK"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "037A2BABF19D"),
            HOMEKIT_CHARACTERISTIC(MODEL, "MyLED"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, led_identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sample LED"),
            HOMEKIT_CHARACTERISTIC(
              ON, false,
              .getter=led_on_get,
              .setter=led_on_set
            ),
            HOMEKIT_CHARACTERISTIC(
              BRIGHTNESS, 100,
              .getter = led_brightness_get,
              .setter = led_brightness_set
            ),
            HOMEKIT_CHARACTERISTIC(
              HUE, 0,
              .getter = led_hue_get,
              .setter = led_hue_set
            ),
            HOMEKIT_CHARACTERISTIC(
              SATURATION, 0,
              .getter = led_saturation_get,
              .setter = led_saturation_set
            ),
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
  .accessories = accessories,
  .password = "111-11-111"
};

void on_wifi_ready() {
  homekit_server_init(&config);
}

void app_main(void) {
  // Initialize led strip
  gpioSetup(16, OUTPUT, LOW);

  if (digitalLeds_initStrands(STRANDS, STRANDCNT)) {
    ets_printf("Init FAILURE: halting\n");
    while (true) {};
  }

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK( ret );

  wifi_init();
}
