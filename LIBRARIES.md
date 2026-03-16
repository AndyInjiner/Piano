# Необходимые библиотеки Arduino

Для работы проекта ESP32 Piano Trainer требуются следующие библиотеки:

## Установка через Library Manager

Откройте Arduino IDE → Tools → Manage Libraries и установите:

### 1. TFT_eSPI (версия 2.5.0+)
- **Автор:** Bodmer
- **Ссылка:** https://github.com/Bodmer/TFT_eSPI
- **Примечание:** После установки скопируйте файл `User_Setup.h` из проекта в папку библиотеки

### 2. FastLED (версия 3.5.0+)
- **Автор:** Daniel Garcia
- **Ссылка:** https://github.com/FastLED/FastLED

### 3. WebSockets (версия 2.3.0+)
- **Автор:** Markus Sattler (Links2004)
- **Ссылка:** https://github.com/Links2004/arduinoWebSockets

### 4. ArduinoJson (версия 6.21.0+)
- **Автор:** Benoit Blanchon
- **Ссылка:** https://github.com/bblanchon/ArduinoJson

## Установка вручную

Скачайте библиотеки и распакуйте в папку `Documents/Arduino/libraries/`:

1. https://github.com/Bodmer/TFT_eSPI/archive/refs/heads/master.zip
2. https://github.com/FastLED/FastLED/archive/refs/heads/master.zip
3. https://github.com/Links2004/arduinoWebSockets/archive/refs/heads/master.zip
4. https://github.com/bblanchon/ArduinoJson/archive/refs/heads/6.x.zip

## Настройка TFT_eSPI

После установки TFT_eSPI:

1. Найдите папку библиотеки (обычно `Documents/Arduino/libraries/TFT_eSPI/`)
2. Скопируйте файл `User_Setup.h` из проекта в эту папку
3. ИЛИ отредактируйте файл `User_Setup_Select.h` в папке библиотеки:
   - Закомментируйте `#include <User_Setups/Setup25_ILI9341.h>`
   - Раскомментируйте `#include <User_Setup.h>`

## Проверка установки

Откройте пример для каждой библиотеки:

- TFT_eSPI: File → Examples → TFT_eSPI → Test and diagnostics
- FastLED: File → Examples → FastLED → Blink
- WebSockets: File → Examples → WebSocketsServer → Basic

## Для ESP32 board

Убедитесь, что установлена плата ESP32:

1. File → Preferences
2. Добавьте в "Additional Boards Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Tools → Board → Boards Manager
4. Найдите "ESP32" и установите "ESP32 by Espressif Systems"

## Версии библиотек

Проверено с версиями:
- TFT_eSPI: 2.5.43
- FastLED: 3.7.0
- WebSockets: 2.4.1
- ArduinoJson: 6.21.3
- ESP32 Core: 2.0.9

## Возможные проблемы

### Ошибка "TFT_eSPI was not declared in this scope"
- Убедитесь, что файл `User_Setup.h` скопирован правильно

### Ошибка компиляции с FastLED
- Попробуйте версию 3.5.0 вместо более новых

### Ошибка "littlefs.h not found"
- Убедитесь, что установлена плата ESP32 2.0+

### Ошибка подключения WebSocket
- Проверьте, что библиотека WebSockets от Links2004, не путать с другой
