# Датчик температуры и влажности для Apple Home

В этом репозитории вы найдете прошивку и схемы для датчика температуры и влажности с дисплеем, который может быть использолван с Apple Home. Видео про это устройство можно посмотреть по [этой](https://youtu.be/CxgiTkTlNZg) ссылке.  
 
**Используемые компоненты**

- Датчик температуры и влажности DHT11
- OLED SSD1306 128x64 i2c
- ESP32C3FN4 Super Mini
- Резистор 10K
- Кнопка
- Источник питания 5V 1A
 
**Используемые библиотеки Arduino**

- esp32 by Espressif Systems (board) 3.3.7
- HomeSpan 2.1.7
- Adafruit GFX Library 1.12.4
- Adafruit SSD1306 2.5.16
 
**Настройки Arduino IDE**

- Board: ESP32C3 Dev BModule
- ESP CDC On Boot: Enabled
- CPU Frequency: 160MHz (WiFi)
- Core Debug Level: None
- Erase All Flash Before Sketch Upload: Disabled
- Flash frequency: 80Mhz
- Flash Mode: QIO
- Flash Size: 4MB (32Mb)
- JTAG Adapter: Disabled
- Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
- Upload Speed: 921600
- Zigbee Mode: Disabled
- Programmer: Esptool

**Поддержать автора**

Если вам нравятся мои проекты и видео, вы можете поддержать меня используя ссылки ниже:

**BuyMeACoffee**: https://buymeacoffee.com/dronetales  
**Boosty**: https://boosty.to/drone_tales/donate  

**BTC**: bitcoin:1A1WM3CJzdyEB1P9SzTbkzx38duJD6kau  
**BCH**: bitcoincash:qre7s8cnkwx24xpzvvfmqzx6ex0ysmq5vuah42q6yz  
**ETH**: 0xf780b3B7DbE2FC74b5F156cBBE51F67eDeAd8F9a  
