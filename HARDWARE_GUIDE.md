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
- **USB чип**: CH340 (порт /dev/cu.usbserial-110 на macOS)

### Распиновка ESP32 → HUB75

| Сигнал   | GPIO | Описание                          |
|----------|------|-----------------------------------|
| R1 (MOSI)| 13   | Данные красного (SPI MOSI)        |
| CLK      | 14   | Тактовый сигнал (SPI CLK)         |
| A        | 19   | Адресная линия 0                  |
| B        | 23   | Адресная линия 1                  |
| C        | 18   | Адресная линия 2 (НЕ используется для 1/4 scan, не подключён) |
| STB/LAT  | 22   | Latch — защёлка данных            |
| P_OE     | 16   | Output Enable                     |

**Важно**: пины CLK=14 и R1=13 — это дефолтные SPI пины PxMatrix для ESP32. Они НЕ передаются в конструктор, а используются автоматически через SPI.

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
upload_speed = 460800       ; 921600 вызывает ошибки на этом кабеле/чипе
lib_deps =
    adafruit/Adafruit GFX Library@^1.11.9
    2dom/PxMatrix LED MATRIX library@^1.8.2
    fastled/FastLED@^3.6.0  ; нужна для Aurora эффектов, можно убрать если не используешь
```

### Библиотеки
1. **PxMatrix** (2dom/PxMatrix LED MATRIX library) — драйвер HUB75 панелей. Наследует Adafruit GFX, даёт drawPixel, drawLine, print и т.д.
2. **Adafruit GFX** — базовая графическая библиотека (шрифты, примитивы). Зависимость PxMatrix.
3. **FastLED** — нужна только если используешь Aurora demo эффекты. Для кастомных эффектов не обязательна.

## Критические настройки дисплея

```cpp
// Обязательно ДО #include <PxMatrix.h>
#define PxMATRIX_double_buffer true   // двойная буферизация, без неё мерцание

// Конструктор: ширина, высота, LAT, OE, адресные пины
PxMATRIX display(32, 16, P_LAT, P_OE, P_A, P_B);
//                                          ^^^^
//  Только A и B — для 1/4 scan на 16 строк (2 адресных линии = 4 группы)
//  НЕ передавать P_C — он не подключён!

// В setup():
display.begin(4);                  // 1/4 scan — ОБЯЗАТЕЛЬНО 4, не 8!
display.setFastUpdate(true);       // быстрое обновление
display.setScanPattern(ZAGZIG);    // ИМЕННО ZAGZIG для этой панели
// НЕ вызывать setPanelsWidth() — у нас одна панель (default = 1)
```

### Что было пробовано и НЕ работает:
- `display(64, 16, ...)` с `setPanelsWidth(2)` — дублирование/артефакты (это для 2 панелей)
- `begin(8)` — показывает чётко но дублирует верх/низ
- Другие scan pattern (LINE, ZIGZAG, ZAGGIZ и т.д.) — артефакты

## Обновление дисплея (ISR)

Дисплей обновляется через аппаратный таймер ESP32 каждые 2мс:

```cpp
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t display_draw_time = 1;  // время свечения в мкс (больше = ярче, но может крашить)

void IRAM_ATTR display_updater() {
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Включить обновление:
timer = timerBegin(0, 80, true);           // таймер 0, делитель 80 (1МГц)
timerAttachInterrupt(timer, &display_updater, true);
timerAlarmWrite(timer, 2000, true);        // каждые 2000мкс = 2мс
timerAlarmEnable(timer);
```

**Внимание**: это API для ESP32 Arduino Core 2.x. В Core 3.x API таймеров изменился!

## Паттерн рисования (game loop)

```cpp
void loop() {
  display.clearDisplay();       // очистить буфер (не экран!)
  // ... рисуем пиксели, текст, линии ...
  display.showBuffer();         // показать буфер на экране (swap)
  delay(20);                    // задержка кадра
}
```

- `clearDisplay()` — очищает задний буфер
- `showBuffer()` — переключает буферы (double buffering)
- Без double buffer: рисуем прямо в display, без showBuffer()

## Цвета

PxMatrix использует 16-bit RGB565:
```cpp
uint16_t color = display.color565(R, G, B);  // R,G,B: 0-255
```

HSV → RGB565 конвертер есть в коде (функция `hsv()`).

## Текст (Adafruit GFX)

```cpp
display.setTextWrap(false);        // отключить перенос строк
display.setTextSize(1);            // 1 = 6x8 пикселей на символ
display.setTextColor(color);
display.setCursor(x, y);           // x,y = верхний левый угол
display.print("Text");
```

Размеры текста:
- Size 1: 6x8 px на символ. "Qoima" = 30px шириной
- Size 2: 12x16 px на символ — ЗАПОЛНЯЕТ ВСЮ высоту 16px дисплея

## Структура проекта

```
matrix/
├── platformio.ini          # конфигурация проекта
├── src/
│   └── main.cpp            # основной код с эффектами
├── include/                # (пусто)
├── lib/                    # (пусто)
└── .pio/                   # сборка, зависимости (автогенерация)
```

## Команды

```bash
# Сборка
~/.platformio/penv/bin/pio run

# Сборка + прошивка
~/.platformio/penv/bin/pio run -t upload

# Serial монитор
~/.platformio/penv/bin/pio device monitor
```

## Текущие эффекты в коде

1. **Qoima Intro** — бегущая строка "Qoima" + логотип Q выезжает справа
2. **Matrix Rain** — зелёный дождь из "Матрицы"
3. **Rainbow Plasma** — радужные синусоидальные волны
4. **Fireworks** — фейерверки с физикой гравитации
5. **Fire** — реалистичный огонь (пропагация тепла снизу вверх)
6. **Starfield** — 3D звёздное поле с ускорением
7. **Bouncing Logo** — логотип Qoima прыгает как DVD screensaver
8. **Snake** — змейка с простым AI (идёт к еде, избегает себя)

## Логотип Qoima

Генерируется математически в `computeLogo()` — 14x14 пикселей:
- Толстое кольцо (outer R=5.9, inner R=2.8, центр 5.5,5.5)
- Вырез справа-снизу (gap для хвоста Q, углы 0.25-1.25 рад)
- Квадрат внутри кольца (пиксели 4-6, 4-6)
- Диагональный хвост (от 8,8 до 12,12)
- Маленький квадрат на конце хвоста (11-12, 11-12)
