# Инструкция по сборке Android приложения

## Требования

- Android Studio Arctic Fox (2020.3.1) или новее
- JDK 11 или новее
- Android SDK 33 (или новее)
- Минимальная версия Android: 7.0 (API 24)

## Шаг 1: Создание проекта

1. Откройте Android Studio
2. File → New → New Project
3. Выберите "Empty Activity"
4. Настройте проект:
   - **Name:** PianoTrainer
   - **Package name:** com.pianotrainer.esp32
   - **Language:** Kotlin
   - **Minimum SDK:** API 24 (Android 7.0)
   - **Build configuration language:** Kotlin DSL

## Шаг 2: Настройка зависимостей

### build.gradle (Module: app)

Откройте файл `app/build.gradle` и добавьте зависимости:

```gradle
dependencies {
    implementation 'androidx.core:core-ktx:1.9.0'
    implementation 'androidx.appcompat:appcompat:1.6.1'
    implementation 'com.google.android.material:material:1.8.0'
    implementation 'androidx.constraintlayout:constraintlayout:2.1.4'
    
    // Корутины для асинхронных операций
    implementation 'org.jetbrains.kotlinx:kotlinx-coroutines-android:1.6.4'
    
    // OkHttp для HTTP запросов
    implementation 'com.squareup.okhttp3:okhttp:4.10.0'
}
```

### build.gradle (Project)

Убедитесь, что в корневом `build.gradle` есть:

```gradle
plugins {
    id 'com.android.application' version '8.0.0' apply false
    id 'org.jetbrains.kotlin.android' version '1.8.0' apply false
}
```

## Шаг 3: Копирование файлов

1. Скопируйте `MainActivity.kt` в `app/src/main/java/com/pianotrainer/esp32/`
2. Скопируйте `activity_main.xml` в `app/src/main/res/layout/`
3. Скопируйте `AndroidManifest.xml` в `app/src/main/`

## Шаг 4: Настройка AndroidManifest.xml

Убедитесь, что добавлены разрешения:

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
```

## Шаг 5: Сборка APK

1. Build → Build Bundle(s) / APK(s) → Build APK(s)
2. APK будет сохранён в `app/build/outputs/apk/debug/app-debug.apk`

## Шаг 6: Установка на устройство

### Через USB:
1. Включите отладку по USB на Android устройстве
2. Подключите устройство к компьютеру
3. Run → Run 'app'

### Через APK файл:
1. Скопируйте APK файл на устройство
2. Откройте файл через файловый менеджер
3. Разрешите установку из неизвестных источников
4. Установите приложение

## Использование приложения

1. **Подключение к ESP32:**
   - Откройте настройки WiFi на Android устройстве
   - Подключитесь к сети "PianoTrainer"
   - Пароль: `piano2024`

2. **Загрузка MIDI файла:**
   - Откройте приложение
   - Нажмите "📁 Загрузить MIDI файл"
   - Выберите MIDI файл на устройстве

3. **Воспроизведение:**
   - Выберите файл из списка
   - Нажмите "▶ PLAY"
   - Используйте slider для изменения скорости

## Конвертация MP3 в MIDI

Перед загрузкой в приложение, конвертируйте MP3 файлы в MIDI:

1. Откройте https://basicpitch.spotify.com
2. Загрузите MP3 файл
3. Скачайте полученный MIDI файл
4. Загрузите MIDI файл через приложение

## Troubleshooting

### Ошибка "Unable to resolve host"
- Убедитесь, что устройство подключено к WiFi сети ESP32

### Ошибка при загрузке файла
- Проверьте размер файла (должен быть < 3MB)
- Убедитесь, что файл в формате MIDI (.mid, .midi)

### Приложение вылетает
- Проверьте логи через `adb logcat`
- Убедитесь, что разрешения предоставлены

## Дополнительные настройки

### Изменение IP адреса

По умолчанию приложение использует `192.168.4.1` (стандартный IP ESP32 в режиме AP).

Для изменения отредактируйте строку в `MainActivity.kt`:
```kotlin
private var esp32Ip = "192.168.4.1"
```

### Изменение порта

Веб-сервер ESP32 использует порт 80 по умолчанию.
Для изменения порта отредактируйте в `piano.ino`:
```cpp
WebServer server(80);  // Измените на нужный порт
```
