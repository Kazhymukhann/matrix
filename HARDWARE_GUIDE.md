# Qoima LED Matrix — полная документация

## Железо

### LED панель
- **Тип**: P10 outdoor, SMD3535, HUB75
- **Размер пикселей**: 32 x 16 (одна панель)
- **Физический размер**: 320mm x 160mm
- **Pixel pitch**: 10mm
- **Scan type**: 1/4 scan (4 строки мультиплексируются одновременно)
- **Scan pattern**: ZAGZIG (критически важно! другие паттерны дают артефакты)
- **Яркость**: 4500 cd/m²
- **Refresh**: 240-1000Hz
- **Питание панели**: 5V 40A (через отдельный БП)

### Контроллер
- **MCU**: ESP32-D0WD-V3 (Dual Core, 240MHz, 320KB RAM, 4MB Flash)
- **Плата**: ESP32 Dev Module (generic)
- **MAC**: d4:e9:f4:b5:0f:18
- **USB чип**: CH340 (порт /dev/cu.usbserial-110 на macOS)

### Распиновка ESP32 → HUB75 (PxMatrix, одна панель)

| Сигнал   | GPIO | Описание                          |
|----------|------|-----------------------------------|
| R1 (MOSI)| 13   | Данные красного (SPI MOSI)        |
| CLK      | 14   | Тактовый сигнал (SPI CLK)         |
| A        | 19   | Адресная линия 0                  |
| B        | 23   | Адресная линия 1                  |
| STB/LAT  | 22   | Latch — защёлка данных            |
| P_OE     | 16   | Output Enable                     |

**Важно**: пины CLK=14 и R1=13 — дефолтные SPI пины PxMatrix для ESP32.

## Прошивка по воздуху (OTA)

### Как это работает
ESP32 подключается к домашнему WiFi и слушает OTA-обновления. Можно прошивать по IP без USB кабеля.

### Первоначальная настройка (один раз по USB)
1. Залить прошивку с поддержкой OTA через USB:
   ```bash
   # В platformio.ini должно быть upload_protocol = esptool (USB)
   ~/.platformio/penv/bin/pio run -t upload
   ```
2. ESP32 подключится к WiFi и получит IP (смотреть в Serial или роутере)
3. Найти ESP32 в сети:
   ```bash
   ping qoima-matrix.local
   ```

### Дальнейшие прошивки по WiFi
1. В `platformio.ini` включить OTA:
   ```ini
   upload_protocol = espota
   upload_port = 192.168.31.81    ; IP вашего ESP32
   ```
2. Прошивать как обычно:
   ```bash
   ~/.platformio/penv/bin/pio run -t upload
   ```
3. Прошивка идёт по WiFi, USB не нужен!

### Важные моменты OTA
- **WiFi должен подключиться ДО запуска таймера дисплея** — иначе crash (xQueueSemaphoreTake)
- **Дисплей отключается во время OTA** — флаг `otaInProgress` блокирует ISR
- **После OTA ESP32 автоматически перезагружается** с новым кодом
- **Если OTA сломается** — всегда можно прошить обратно по USB
- **mDNS имя**: `qoima-matrix.local` (можно пинговать вместо IP)

### Минимальный код с OTA
```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

const char* WIFI_SSID = "Admin";
const char* WIFI_PASS = "92211667";

void setup() {
  // 1. Подключиться к WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  // 2. Запустить OTA
  ArduinoOTA.setHostname("qoima-matrix");
  ArduinoOTA.begin();

  // 3. Теперь можно запускать дисплей и остальное
}

void loop() {
  ArduinoOTA.handle();  // обязательно в loop!
  // ... остальной код ...
}
```

### Текущий IP
- **WiFi сеть**: Admin
- **IP ESP32**: 192.168.31.81
- **mDNS**: qoima-matrix.local
- **OTA hostname**: qoima-matrix

## Софт

### Платформа
- **IDE**: PlatformIO (CLI или VS Code)
- **Framework**: Arduino для ESP32
- **Путь к pio**: `~/.platformio/penv/bin/pio`

### platformio.ini
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 9600
upload_speed = 460800
lib_deps =
    adafruit/Adafruit GFX Library@^1.11.9
    2dom/PxMatrix LED MATRIX library@^1.8.2

; OTA прошивка по WiFi:
upload_protocol = espota
upload_port = 192.168.31.81
```

### Библиотеки
1. **PxMatrix** (2dom/PxMatrix LED MATRIX library) — драйвер HUB75 панелей (одна панель)
2. **Adafruit GFX** — графическая библиотека (шрифты, примитивы)
3. **ArduinoOTA** — прошивка по WiFi (встроена в ESP32 Arduino Core)

## Критические настройки дисплея

```cpp
#define PxMATRIX_double_buffer true

PxMATRIX display(32, 16, P_LAT, P_OE, P_A, P_B);

display.begin(4);                  // 1/4 scan
display.setFastUpdate(true);
display.setScanPattern(ZAGZIG);
```

### Что было пробовано и НЕ работает:
- `begin(8)` — дублирует верх/низ
- Другие scan patterns (LINE, ZIGZAG и т.д.) — артефакты на одной панели
- **Мульти-панель через PxMatrix** — НЕ РАБОТАЕТ для этих P10 панелей
  - setPanelsWidth(2,3,4) — текст разбросан на все scan patterns
  - 512x16 цепочка — ничего не показывает
  - Причина: PxMatrix применяет scan remapping глобально, а не per-panel

### Мульти-панель: что работает
- **3 панели в линию (96x16)**: цепочка шлейфами OUT→IN подтверждена
- Сплошные цвета отображаются правильно (зелёный/красный/синий на 3 панелях)
- Текст НЕ рендерится корректно из-за scan pattern remapping
- **Для мульти-панель рекомендуется**: ESP32-HUB75-MatrixPanel-I2S-DMA (нужна другая распиновка, 11 проводов вместо 6)

## Обновление дисплея (ISR)

```cpp
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t display_draw_time = 1;

void IRAM_ATTR display_updater() {
  if (otaInProgress) return;  // не обновлять во время OTA!
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Включить обновление:
timer = timerBegin(0, 80, true);
timerAttachInterrupt(timer, &display_updater, true);
timerAlarmWrite(timer, 2000, true);
timerAlarmEnable(timer);
```

**Внимание**: WiFi ДОЛЖЕН подключиться ДО запуска таймера!

## Команды

```bash
# Сборка
~/.platformio/penv/bin/pio run

# Прошивка по USB (первый раз)
# В platformio.ini: upload_protocol = esptool
~/.platformio/penv/bin/pio run -t upload

# Прошивка по WiFi (OTA)
# В platformio.ini: upload_protocol = espota, upload_port = 192.168.31.81
~/.platformio/penv/bin/pio run -t upload

# Стереть flash полностью
~/.platformio/penv/bin/pio run -t erase

# Serial монитор
~/.platformio/penv/bin/pio device monitor

# Найти ESP32 в сети
ping qoima-matrix.local
```

## Структура проекта

```
matrix/
├── platformio.ini              # конфигурация (USB и OTA)
├── HARDWARE_GUIDE.md           # этот файл
├── main_1panel.cpp.bak         # бэкап кода для 1 панели с эффектами
├── src/
│   └── main.cpp                # текущий код (OTA + LED blink)
├── include/
├── lib/
└── .pio/                       # сборка (автогенерация)
```
