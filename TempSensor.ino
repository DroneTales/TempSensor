// ESP32C3FN4 SuperMini Board
// ===============================================================
// Arduino IDE settings:
//   - Board: ESP32C3 Dev BModule
//   - ESP CDC On Boot: Enabled
//   - CPU Frequency: 160MHz (WiFi)
//   - Core Debug Level: None
//   - Erase All Flash Before Sketch Upload: Disabled
//   - Flash frequency: 80Mhz
//   - Flash Mode: QIO
//   - Flash Size: 4MB (32Mb)
//   - JTAG Adapter: Disabled
//   - Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
//   - Upload Speed: 921600
//   - Zigbee Mode: Disabled
//   - Programmer: Esptool
// ===============================================================

#include <driver/rmt_rx.h>

#include <Adafruit_SSD1306.h>
#include <HomeSpan.h>
#include <Fonts/FreeMonoBold18pt7b.h>

#include "Images.h"

/**************************************************************************************/
/*                                 Hardware  settings                                 */

#define STATUS_LED_PIN          GPIO_NUM_8
#define CONTROL_PIN             GPIO_NUM_9

#define DHT_PIN                 GPIO_NUM_10
// Temperature range is 0-50 with +/-2 accuracy.
#define DHT_LOW_TEMPERATURE     -2
#define DHT_HIGH_TEMERATURE     52
// Humidity range is 20-80 with +/-5 accuracy.
#define DHT_LOW_HUMIDITY        15
#define DHT_HIGH_HUMIDITY       85

// OLED pins.
#define OLED_SDA_PIN            GPIO_NUM_4
#define OLED_SCL_PIN            GPIO_NUM_5

// OLED I2C address.
#define OLED_I2C_ADDRESS        0x3C
// OLED dimensin.
#define OLED_SCREEN_WIDTH       128
#define OLED_SCREEN_HEIGHT      64

// Uncomment the line below if you use DHT22
// Comment the line below if you use DHT11
//#define DHT22

#ifndef DHT22
    // If you use DHT11 you can correct its measurement by adding some offset
    #define HUMIDITY_OFFSET     14
    #define TEMPERATURE_OFFSET  (-3)
#endif

/**************************************************************************************/


/**************************************************************************************/
/*                                 Firmware  settings                                 */

// Firmware version.
#define VERSION                 "1.0.0.0"

// How long boot logo should be displayed.
#define LOGO_SHOW_DELAY         2000

// How often data should be read from DHT11 sensor.
#define READ_SENSOR_INTERVAL    5000

// How often data should be updated on the display.
#define UI_UPDATE_INTERVAL      500

// Used to wakeup UI task when status changed.
#define UI_WAKEUP_EVENT         0x01

/**************************************************************************************/


/**************************************************************************************/
/*                                       Objects                                      */

// OLED object.
Adafruit_SSD1306 Oled(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, -1);

/**************************************************************************************/


/**************************************************************************************/
/*                                  Global  variable                                  */

// Last read temperature.
float Temperature = NAN;
// Last read humidity.
float Humidity = NAN;

// Current HomeSpan status.
HS_STATUS HomeSpanStatus = HS_WIFI_NEEDED;

// UI task wakeup event.
EventGroupHandle_t UiTaskWakeupEvent = nullptr;

/**************************************************************************************/


/**************************************************************************************/
/*                                  HomeSpan sensors                                  */

// Device record.
struct DeviceIdentify : Service::AccessoryInformation {
    SpanCharacteristic* FIdentify;
    
    DeviceIdentify(const char* Name, const char* Model) : Service::AccessoryInformation() {
        char Sn[24];
        snprintf(Sn, 24, "DRONETALES-%llX", ESP.getEfuseMac());
        
        new Characteristic::Name(Name);
        new Characteristic::Manufacturer("DroneTales");
        new Characteristic::SerialNumber(Sn);
        new Characteristic::Model(Model);
        new Characteristic::FirmwareRevision(VERSION);
        FIdentify = new Characteristic::Identify();
    }
};

// Temperature sensor.
struct TemperatureSensor : Service::TemperatureSensor {
    SpanCharacteristic* FTemperature;
    
    TemperatureSensor() : Service::TemperatureSensor() {
        FTemperature = new Characteristic::CurrentTemperature(Temperature);
        FTemperature->setRange(DHT_LOW_TEMPERATURE, DHT_HIGH_TEMERATURE);
    }
    
    void loop() {
        if (isnan(Temperature)) {
            FTemperature->setVal(DHT_LOW_TEMPERATURE);
        } else {
            FTemperature->setVal(Temperature);
        }
    }
};

// Humidity sensor.
struct HumiditySensor : Service::HumiditySensor {
    SpanCharacteristic* FHumidity;
    
    HumiditySensor() : Service::HumiditySensor() {
        FHumidity = new Characteristic::CurrentRelativeHumidity(Humidity);
        FHumidity->setRange(DHT_LOW_HUMIDITY, DHT_HIGH_HUMIDITY);
    }
    
    void loop() {
        if (isnan(Humidity)) {
            FHumidity->setVal(DHT_LOW_HUMIDITY);
        } else {
            FHumidity->setVal(Humidity);
        }
    }
};

/**************************************************************************************/


/**************************************************************************************/
/*                                  Helper functions                                  */

bool IsEqual(const float a, const float b) {
    return ((isnan(a) && isnan(b)) || (a == b));
}

/**************************************************************************************/


/**************************************************************************************/
/*                                    UI functions                                    */

// Draws the bitmap in the screen center.
void DrawBitmap(const int16_t Width, const int16_t Height, const uint8_t Bitmap[]) {
    Oled.drawBitmap((OLED_SCREEN_WIDTH - Width) / 2, (OLED_SCREEN_HEIGHT - Height) / 2,
        Bitmap, Width, Height, WHITE);
}

// Shows the current sensor data. Do it only if the HomeSpan status is HS_PAIRED.
// FirstTime should be set to true only when the function called from HomeSpan status
// changed callback.
void DisplaySensorData(const bool FirstTime) {
    static float LastTemperature = NAN;
    static float LastHumidity = NAN;
    static bool ShowSign = true;

    // Update last data if it was changed.
    bool DataUpdated = (!IsEqual(Temperature, LastTemperature) ||
        !IsEqual(Humidity, LastHumidity));
    if (DataUpdated) {
        LastTemperature = Temperature;
        LastHumidity = Humidity;
    }
    
    // Update full UI only if data changed or data shown for the first time.
    if (FirstTime || DataUpdated) {
        // Clear display.
        Oled.clearDisplay();

        // Draw the sensor image.
        DrawBitmap(SENSOR_BMP_WIDTH, SENSOR_BMP_HEIGHT, SENSOR_BMP);
        
        // Setup the font and text color.
        Oled.setFont(&FreeMonoBold18pt7b);
        Oled.setTextColor(WHITE);
        
        // Display temperature value.
        Oled.setCursor(45, 28);
        if (isnan(Temperature)) {
            Oled.print("--");
        } else {
            Oled.print((int8_t)Temperature);
        }
        Oled.setCursor(100, 27);
        Oled.print("C");
        
        // Display humidity value.
        Oled.setCursor(45, 62);
        if (isnan(Humidity)) {
            Oled.print("--");
        } else {
            Oled.print((uint8_t)Humidity);
        }
        Oled.print("%");

        // Display the data.
        Oled.display();
    }

    if (FirstTime) {
        // If data shown for the first time we must display the sigh.
        ShowSign = true;
    } else {
        // Otherwise blink it.
        ShowSign = !ShowSign;
    }

    // Draw the sign.
    if (ShowSign) {
        Oled.drawCircle(92, 8, 3, WHITE);
    } else {
        Oled.drawCircle(92, 8, 3, BLACK);
    }
    Oled.display();
}

// Draws the status icon.
void DrawStatus(const int16_t Width, const int16_t Height, const uint8_t Bitmap[]) {
    Oled.clearDisplay();
    DrawBitmap(Width, Height, Bitmap);
    Oled.display();
}

// Simple clears the display.
void ClearDisplay() {
    Oled.clearDisplay();
    Oled.display();
}

// Shows the initial status icon.
void DisplayInitialIcon(const HS_STATUS Status) {
    switch (Status) {
        // WiFi Credentials have not yet been set/stored.
        case HS_WIFI_NEEDED:
            DrawStatus(WIFI_NEEDED_BMP_WIDTH, WIFI_NEEDED_BMP_HEIGHT, WIFI_NEEDED_BMP);
            break;
            
        // HomeSpan is trying to connect to the network specified in the stored WiFi
        // Credentials.
        case HS_WIFI_CONNECTING:
            DrawStatus(WIFI_CONNECTING_BMP_WIDTH, WIFI_CONNECTING_BMP_HEIGHT,
                WIFI_CONNECTING_BMP[0]);
            break;
            
        // HomeSpan is connected to central WiFi network, but device has not yet been
        // paired to HomeKit.
        case HS_PAIRING_NEEDED:
            DrawStatus(PAIRING_NEEDED_BMP_WIDTH, PAIRING_NEEDED_BMP_HEIGHT,
                PAIRING_NEEDED_BMP);
            break;
            
        // HomeSpan is connected to central WiFi network and ther device has been
        // paired to HomeKit.
        case HS_PAIRED:
            // Simple ignore the status. Should never be here.
            break;
            
        // User has requested the device to enter into Command Mode.
        case HS_ENTERING_CONFIG_MODE:
            DrawStatus(ENTER_CONFIG_BMP_WIDTH, ENTER_CONFIG_BMP_HEIGHT,
                ENTER_CONFIG_BMP);
            break;
            
        // HomeSpan is in Command Mode with "Exit Command Mode" specified as choice.
        case HS_CONFIG_MODE_EXIT:
            DrawStatus(CONFIG_EXIT_BMP_WIDTH, CONFIG_EXIT_BMP_HEIGHT, CONFIG_EXIT_BMP);
            break;

        // HomeSpan is in Command Mode with "Reboot" specified as choice.
        case HS_CONFIG_MODE_REBOOT:
            DrawStatus(REBOOT_BMP_WIDTH, REBOOT_BMP_HEIGHT, REBOOT_BMP);
            break;
            
        // HomeSpan is in Command Mode with "Launch Access Point" specified
        // as choice.
        case HS_CONFIG_MODE_LAUNCH_AP:
            DrawStatus(LAUNCH_AP_BMP_WIDTH, LAUNCH_AP_BMP_HEIGHT, LAUNCH_AP_BMP);
            break;
            
        // HomeSpan is in Command Mode with "Unpair Device" specified as choice.
        case HS_CONFIG_MODE_UNPAIR:
            DrawStatus(UNPAIR_BMP_WIDTH, UNPAIR_BMP_HEIGHT, UNPAIR_BMP);
            break;

        // HomeSpan is in Command Mode with "Erase WiFi Credentials" specified as choice.
        case HS_CONFIG_MODE_ERASE_WIFI:
            DrawStatus(ERASE_WIFI_BMP_WIDTH, ERASE_WIFI_BMP_HEIGHT, ERASE_WIFI_BMP);
            break;

        // User has selected "Exit Command Mode".
        case HS_CONFIG_MODE_EXIT_SELECTED:
            // We need to clear display because the status icon was elready shown.
            ClearDisplay();
            break;
            
        // User has select "Reboot" from the Command Mode.
        case HS_CONFIG_MODE_REBOOT_SELECTED:
            // We need to clear display because the status icon was elready shown.
            ClearDisplay();
            break;
            
        // User has selected "Launch AP Access" from the Command Mode.
        case HS_CONFIG_MODE_LAUNCH_AP_SELECTED:
            // We need to clear display because the status icon was elready shown.
            ClearDisplay();
            break;

        // User has seleected "Unpair Device" from the Command Mode.
        case HS_CONFIG_MODE_UNPAIR_SELECTED:
            // We need to clear display because the status icon was elready shown.
            ClearDisplay();
            break;

        // User has selected "Erase WiFi Credentials" from the Command Mode.
        case HS_CONFIG_MODE_ERASE_WIFI_SELECTED:
            // We need to clear display because the status icon was elready shown.
            ClearDisplay();
            break;
            
        // HomeSpan is in the process of rebooting the device.
        case HS_REBOOTING:
            DrawStatus(REBOOTING_BMP_WIDTH, REBOOTING_BMP_HEIGHT, REBOOTING_BMP);
            break;
            
        // HomeSpan is in the process of performing a Factory Reset of device.
        case HS_FACTORY_RESET:
            DrawStatus(RESET_BMP_WIDTH, RESET_BMP_HEIGHT, RESET_BMP);
            break;
            
        // HomeSpan has started the Access Point but no one has yet connected.
        case HS_AP_STARTED:
            DrawStatus(AP_STARTED_BMP_WIDTH, AP_STARTED_BMP_HEIGHT, AP_STARTED_BMP[0]);
            break;
            
        // The Access Point is started and a user device has been connected.
        case HS_AP_CONNECTED:
            DrawStatus(AP_CONNECTED_BMP_WIDTH, AP_CONNECTED_BMP_HEIGHT, AP_CONNECTED_BMP);
            break;

        // HomeSpan has terminated the Access Point.
        case HS_AP_TERMINATED:
            DrawStatus(AP_TERMINATED_BMP_WIDTH, AP_TERMINATED_BMP_HEIGHT, AP_TERMINATED_BMP);
            break;
            
        // HomeSpan is in the process of receiving an Over-the-Air software update.
        case HS_OTA_STARTED:
            // Should never happen.
            break;
        
        // HomeSpan is in the process of scanning for WiFi networks.
        case HS_WIFI_SCANNING:
            DrawStatus(WIFI_SCANNING_BMP_WIDTH, WIFI_SCANNING_BMP_HEIGHT, WIFI_SCANNING_BMP);
            break;

        // HomeSpan is trying to connect to an Ethernet network.
        case HS_ETH_CONNECTING:
            // Should never happen.
            break;
    }
}

void DisplayBlinkableIcons(const bool Hide) {
    if (Hide) {
        Oled.clearDisplay();
        Oled.display();
        return;
    }
    
    switch (HomeSpanStatus) {
        case HS_CONFIG_MODE_EXIT_SELECTED:
            DisplayInitialIcon(HS_CONFIG_MODE_EXIT);
            break;

        case HS_CONFIG_MODE_REBOOT_SELECTED:
            DisplayInitialIcon(HS_CONFIG_MODE_REBOOT);
            break;

        case HS_CONFIG_MODE_LAUNCH_AP_SELECTED:
            DisplayInitialIcon(HS_CONFIG_MODE_LAUNCH_AP);
            break;

        case HS_CONFIG_MODE_UNPAIR_SELECTED:
            DisplayInitialIcon(HS_CONFIG_MODE_UNPAIR);
            break;
            
        case HS_CONFIG_MODE_ERASE_WIFI_SELECTED:
            DisplayInitialIcon(HS_CONFIG_MODE_ERASE_WIFI);
            break;
    }
}

void DisplayHomeSpanStatus(const bool FirstTime) {
    static uint8_t CurrentImageIndex = 0;
    
    // There are some statuses that should be displayed only once
    // and we do not need to update them. Do it only when the
    // FirstTime parameter is true.
    if (FirstTime) {
        // Show the initial icon for the new status.
        DisplayInitialIcon(HomeSpanStatus);
        // Reset image index.
        CurrentImageIndex = 0;
        // And exit.
        return;
    }
    
    // We are here if the method was called not for first time. So all we need to do
    // is just blink a display for some statuses.

    // Make sure our status is animated or blinkable.
    bool Animated = (HomeSpanStatus == HS_WIFI_CONNECTING || HomeSpanStatus == HS_AP_STARTED);
    bool Blinkable = (HomeSpanStatus == HS_CONFIG_MODE_EXIT_SELECTED ||
        HomeSpanStatus == HS_CONFIG_MODE_REBOOT_SELECTED ||
        HomeSpanStatus == HS_CONFIG_MODE_LAUNCH_AP_SELECTED ||
        HomeSpanStatus == HS_CONFIG_MODE_UNPAIR_SELECTED ||
        HomeSpanStatus == HS_CONFIG_MODE_ERASE_WIFI_SELECTED);
    // If status is static one then exit.
    if (!Animated && !Blinkable) {
        return;
    }

    // Is is blinkable icon?
    if (Blinkable) {
        // We are here only in case the status is blinkable.
        CurrentImageIndex = CurrentImageIndex + 1;
        if (CurrentImageIndex > 1) {
            CurrentImageIndex = 0;
        }
        
        DisplayBlinkableIcons(CurrentImageIndex == 0);
        return;
    }

    // We are here only if the icon is animated one.
    CurrentImageIndex = CurrentImageIndex + 1;
    if (CurrentImageIndex > 3) {
        CurrentImageIndex = 0;
    }
    
    if (HomeSpanStatus == HS_WIFI_CONNECTING) {
        DrawStatus(WIFI_CONNECTING_BMP_WIDTH, WIFI_CONNECTING_BMP_HEIGHT,
            WIFI_CONNECTING_BMP[CurrentImageIndex]);
        return;
    }
    
    DrawStatus(AP_STARTED_BMP_WIDTH, AP_STARTED_BMP_HEIGHT,
        AP_STARTED_BMP[CurrentImageIndex]);
}

/**************************************************************************************/


/**************************************************************************************/
/*                                      UI  task                                      */

void UiTask(void *pvParameter) {
    while (true) {
        // Wait for event signal or for timeout. Actually we are interested only
        // in case when status changed.
        EventBits_t Events = xEventGroupWaitBits(UiTaskWakeupEvent,
            UI_WAKEUP_EVENT, pdTRUE, pdFALSE,
            pdMS_TO_TICKS(UI_UPDATE_INTERVAL));
        
        // If the UI_WAKEUP_EVENT signaled then the task woke up from
        // status update callback so status has just been changed and the
        // UI should be updated completely (for the first time).
        bool FirstTime = ((Events & UI_WAKEUP_EVENT) != 0);
        if (HomeSpanStatus == HS_PAIRED) {
            DisplaySensorData(FirstTime);
        } else {
            DisplayHomeSpanStatus(FirstTime);
        }
    }

    vTaskDelete(NULL);
}

/**************************************************************************************/


/**************************************************************************************/
/*                                 Sensor reading task                                */

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
    #define MAX_BLOCKS	64
#else
    #define MAX_BLOCKS	48
#endif

#ifdef DHT22
    #define DHT_DELAY   2
#else
    #define DHT_DELAY   22
#endif

bool IRAM_ATTR DhtRxDone(rmt_channel_handle_t Channel, const rmt_rx_done_event_data_t* EventData, void* UserData) {
    BaseType_t Res = pdFALSE;
    QueueHandle_t Queue = (QueueHandle_t)UserData;
    xQueueSendFromISR(Queue, EventData, &Res);

	return (Res == pdTRUE);
}

// RMT configuration. Place it here to save task stack.
rmt_receive_config_t RxConfig = {
    .signal_range_min_ns = 3000,
    .signal_range_max_ns = 150000
};

rmt_rx_channel_config_t RxChannelConfig = {
    .gpio_num = DHT_PIN,
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 1000000,
    .mem_block_symbols = MAX_BLOCKS
};

rmt_rx_event_callbacks_t Cbs = {
    .on_recv_done = DhtRxDone
};

// Declare buffer variables also here so it saves the task state.
rmt_rx_done_event_data_t RxData = { 0 };
rmt_symbol_word_t Symbols[MAX_BLOCKS] = { 0 };

void ReadSensorTask(void* pvParameter) {
    // Initialize RMT.
    rmt_channel_handle_t RxChannel = nullptr;
    rmt_new_rx_channel(&RxChannelConfig, &RxChannel);
    
    // Create queue and register a RMT callback.
    QueueHandle_t RxQueue = xQueueCreate(1, sizeof(RxData));
    rmt_rx_register_event_callbacks(RxChannel, &Cbs, RxQueue);

    // Setup DHT pin.
    gpio_set_level(DHT_PIN, 1);
    gpio_pullup_dis(DHT_PIN);
    gpio_pulldown_dis(DHT_PIN);
    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_intr_type(DHT_PIN, GPIO_INTR_DISABLE);

    // Enable RMT.
    rmt_enable(RxChannel);
    
    while (true) {
        memset(&RxData, sizeof(RxData), 0);
        memset(Symbols, sizeof(Symbols), 0);

        gpio_set_level(DHT_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(DHT_DELAY));
        gpio_set_level(DHT_PIN, 1);
        
        if (rmt_receive(RxChannel, Symbols, sizeof(Symbols), &RxConfig) == ESP_OK) {
            if (xQueueReceive(RxQueue, &RxData, pdMS_TO_TICKS(100)) == pdPASS) {
                size_t Len = RxData.num_symbols;
                rmt_symbol_word_t* Cur = RxData.received_symbols;
                uint8_t Pulse = Cur[0].duration0 + Cur[0].duration1;
                if (Len >= 41 && Len <= 42 && Pulse >= 130 && Pulse <= 180) {
                    uint8_t Data[6];
                    bool Error = false;
                    for (uint8_t i = 0; i < 40; i++) {
                        Pulse = Cur[i + 1].duration0 + Cur[i + 1].duration1;
                        Error = (Pulse <= 55 || Pulse >= 145);
                        if (Error) {
                            break;
                        }
                        
                        Data[i / 8] <<= 1;
                        if (Pulse > 110) {
                            Data[i / 8] |= 1;
                        }
			        }
			        
                    if (!Error) {
                        uint8_t Total = Data[0] + Data[1] + Data[2] + Data[3];
                        if (Data[4] == Total) {
                            #ifdef DHT22
                                Humidity = (Data[0] * 256 + Data[1]) * 0.1;
                                Temperature = ((Data[2] & 0x7f) * 256 + Data[3]) * 0.1;
                                if (Data[2] & 0x80)
                                    Temperature = -Temperature;
                            #else
                                Humidity = Data[0] + HUMIDITY_OFFSET;
                                Temperature = Data[2] + TEMPERATURE_OFFSET;
                            #endif
                        }
                    }
                }
            }
        }

        gpio_set_level(DHT_PIN, 1);

        delay(READ_SENSOR_INTERVAL);
    }
    
    vQueueDelete(RxQueue);

    rmt_disable(RxChannel);
	rmt_del_channel(RxChannel);

    vTaskDelete(NULL);
}

/**************************************************************************************/


/**************************************************************************************/
/*                                 HomeSpan callbacks                                 */

// This method called when a HomeSpan status has been changed. It will never called
// twice for the same status.
void HomeSpanStatusUpdate(HS_STATUS Status) {
    // Store current status.
    HomeSpanStatus = Status;
    
    // Wakeup UI updater.
    xEventGroupSetBits(UiTaskWakeupEvent, UI_WAKEUP_EVENT);
}

/**************************************************************************************/


/**************************************************************************************/
/*                                  Arduino routines                                  */

void setup() {
    // Initialize UART.
    Serial.begin(115200);
    delay(500);

    // Initialize pins.
    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(CONTROL_PIN, INPUT);
    
    digitalWrite(STATUS_LED_PIN, HIGH);

    // Initialize I2C bus.
    Wire.end();
    Wire.setPins(OLED_SDA_PIN, OLED_SCL_PIN);
    Wire.begin();

    // Initialize OLED display.
    Oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);

    // Show logo.
    Oled.clearDisplay();
    DrawBitmap(LOGO_BMP_WIDTH, LOGO_BMP_HEIGHT, LOGO_BMP);
    Oled.display();
    delay(LOGO_SHOW_DELAY);

    // Create UI task wakeup event.
    UiTaskWakeupEvent = xEventGroupCreate();

    // Start UI task.
    xTaskCreate(UiTask, "UI task", 4000, nullptr, tskIDLE_PRIORITY, nullptr);
    // Start sensor reading task.
    xTaskCreate(ReadSensorTask, "Sensor reading", 4000, nullptr,
        tskIDLE_PRIORITY, nullptr);

    // Initialize HomeSpan.
    homeSpan.setControlPin(CONTROL_PIN);
    homeSpan.setStatusPin(STATUS_LED_PIN);
    homeSpan.setPairingCode("62905821");
    homeSpan.setStatusCallback(HomeSpanStatusUpdate);

    homeSpan.begin(Category::Bridges, "DroneTales Smart Bridge");

    // Add bridge device.
    new SpanAccessory();
    new DeviceIdentify("DroneTales HomeKit", "DroneTales Bridge");
    new Service::HAPProtocolInformation();
    new Characteristic::Version("1.0.0");

    // Add temperature sensor device.
    new SpanAccessory();
    new DeviceIdentify("DroneTales Temperature Sensor", "DHT");
    new TemperatureSensor();

    // Add humidity sensor device.
    new SpanAccessory();
    new DeviceIdentify("DroneTales Humidity Sensor", "DHT");
    new HumiditySensor();
}

void loop() {
    homeSpan.poll();
}

/**************************************************************************************/