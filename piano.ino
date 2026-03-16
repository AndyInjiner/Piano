/*
 * ESP32 Piano Trainer - Обучающее устройство для игры на пианино
 *
 * Компоненты:
 * - ESP32 DevKit
 * - TFT дисплей 240x320 (ILI9341) с сенсорным экраном (XPT2046)
 * - WS2811 LED лента (54 светодиода)
 * - MAX98357A I2S DAC (аудиовыход)
 *
 * Функционал:
 * - Веб-сервер для загрузки MIDI файлов
 * - Парсинг MIDI файлов
 * - Управление LED лентой по нотам
 * - Вывод звука через MAX98357A (I2S)
 * - TFT интерфейс с кнопками play, stop, select, mute и slider скорости
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>
#include <driver/i2s.h>
#include <Preferences.h>

// ==================== WiFi настройки ====================
#ifndef APSSID
#define APSSID "PianoTrainer"
#define APPSK  "piano2024"
#endif

const char* ssid = APSSID;
const char* password = APPSK;

// ==================== TFT дисплей ====================
// Пины для TFT ILI9341
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17
#define TFT_MOSI 23
#define TFT_SCLK 18

// Пины для тачскрина XPT2046
#define TOUCH_CS 19
#define TOUCH_IRQ 25

TFT_eSPI tft = TFT_eSPI();

// ==================== WS2811 LED лента ====================
#define LED_PIN     2
#define NUM_LEDS    54
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

// ==================== MAX98357A I2S DAC ====================
// Пины I2S для MAX98357A
#define I2S_BCLK    26   // Bit Clock
#define I2S_LRC     27   // Left/Right Clock (Word Select)
#define I2S_DOUT    14   // Data Out
#define I2S_PORT    I2S_NUM_0

// Частота дискретизации
#define SAMPLE_RATE 44100
#define SAMPLE_BITS 16

bool soundEnabled = true;  // Вкл/выкл звук
float volume = 0.5f;       // Громкость (0.0 - 1.0)

// Preferences для сохранения настроек
Preferences preferences;

// ==================== MIDI данные ====================
struct NoteEvent {
  uint8_t note;      // номер ноты (0-127)
  uint8_t velocity;  // сила нажатия (0-127)
  uint32_t startTime; // время начала (мс)
  uint32_t duration;  // длительность (мс)
  uint32_t endTime;   // время окончания (мс)
};

struct MidiFile {
  String name;
  std::vector<NoteEvent> notes;
  uint32_t totalDuration;
};

std::vector<MidiFile> midiFiles;
int currentFileIndex = -1;
bool isPlaying = false;
float playbackSpeed = 1.0;
uint32_t playbackStartTime = 0;

// Активные ноты для воспроизведения звука
struct ActiveNote {
  uint8_t note;
  uint32_t endTime;
};
std::vector<ActiveNote> activeNotes;

// ==================== Веб-сервер ====================
WebServer server(80);
WebSocketsServer websocket(81);

// ==================== Прототипы функций ====================
void setupWiFi();
void setupWebServer();
void setupTFT();
void setupLEDs();
void setupI2S();
void handleRoot();
void handleUpload();
void handleFileList();
void handlePlay();
void handleStop();
void handleSpeed();
void handleMute();
void processMIDIFile(File file);
void drawInterface();
void drawFileCarousel();
void drawButtons();
void checkTouch();
void updateLEDs();
uint8_t noteToLED(uint8_t note);
void noteOn(uint8_t note, uint8_t velocity);
void noteOff(uint8_t note);
void playTone(uint8_t note, uint8_t velocity);
void stopTone(uint8_t note);
void toggleSound();
void saveSettings();
void loadSettings();

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);

  // Инициализация LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  Serial.println("LittleFS mounted");

  // Загрузка сохранённых настроек
  loadSettings();

  setupWiFi();
  setupTFT();
  setupLEDs();
  setupI2S();  // Инициализация I2S для звука
  setupWebServer();

  // Загрузка списка MIDI файлов
  loadMidiFileList();

  drawInterface();

  Serial.println("Piano Trainer Ready!");
}

// ==================== LOOP ====================
void loop() {
  server.handleClient();

  if (isPlaying) {
    updateLEDs();
  }

  checkTouch();

  delay(10);
}

// ==================== WiFi ====================
void setupWiFi() {
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
}

// ==================== TFT ====================
void setupTFT() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
}

// ==================== LEDs ====================
void setupLEDs() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  FastLED.clear();
  FastLED.show();
}

// ==================== I2S Звук ====================
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);

  Serial.println("I2S initialized");
}

// ==================== Веб-сервер ====================
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/upload", HTTP_POST, [](WebServer* server) {
    server->send(200);
  }, handleUpload);
  server.on("/files", handleFileList);
  server.on("/play", handlePlay);
  server.on("/stop", handleStop);
  server.on("/speed", handleSpeed);
  server.on("/mute", handleMute);
  server.on("/status", handleStatus);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not Found");
  });

  // Инициализация WebSocket с обработчиком событий
  websocket.begin();
  websocket.onEvent(wsEvent);

  server.begin();
  Serial.println("Web server started");
}

// Обработчик корневого URL - возвращает HTML интерфейс
void handleRoot() {
  String html = F(R"=====(<!DOCTYPE html>
<html lang="ru">
<head>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<meta charset='UTF-8'>
<style>
body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#1a1a2e,#16213e);color:#fff;padding:20px;text-align:center}
h1{color:#00d9ff}
.card{background:rgba(255,255,255,0.1);border-radius:15px;padding:20px;margin:20px auto;max-width:500px}
.btn{background:#00d9ff;color:#1a1a2e;border:none;padding:12px 24px;border-radius:8px;cursor:pointer;font-size:16px;margin:5px}
.btn-play{background:#00aa00;color:#fff}
.btn-stop{background:#aa0000;color:#fff}
.file-item{background:rgba(255,255,255,0.05);padding:12px;margin:8px 0;border-radius:8px;display:flex;justify-content:space-between;align-items:center}
.status{background:rgba(0,217,255,0.2);border-radius:8px;padding:12px;margin-top:15px}
input[type=range]{width:100%}
</style>
<title>ESP32 Piano Trainer</title>
</head>
<body>
<h1>🎹 Piano Trainer</h1>
<div class="card">
<h2>📁 Загрузка MIDI</h2>
<p><a href="https://basicpitch.spotify.com" target="_blank" style="color:#00d9ff;">Конвертируйте MP3→MIDI</a></p>
<form method="POST" action="/upload" enctype="multipart/form-data">
<input type="file" name="midi" accept=".mid,.midi"><br><br>
<button type="submit" class="btn">Загрузить</button>
</form>
</div>
<div class="card">
<h2>📋 Файлы</h2>
<div id="fileList"><p>Загрузка...</p></div>
</div>
<div class="card">
<h2>🎮 Управление</h2>
<button class="btn btn-play" onclick="play()">▶ PLAY</button>
<button class="btn btn-stop" onclick="stop()">⏹ STOP</button>
<label>Скорость: <span id="speedVal">1.0</span>x</label>
<input type="range" min="10" max="200" value="100" oninput="setSpeed(this.value/100)">
</div>
<div class="status"><p id="status">Ожидание...</p></div>
<script>
let selIdx=0,files=[];
function loadFiles(){
  fetch('/files').then(r=>r.json()).then(d=>{
    files=d.files||[];
    let h='';
    if(files.length===0)h='<p>Нет файлов</p>';
    else files.forEach((f,i)=>{h+=`<div class="file-item" onclick="sel(${i})"><span>${f.name}</span><span>${f.notes} нот</span></div>`});
    document.getElementById('fileList').innerHTML=h;
  });
}
function sel(i){selIdx=i;document.querySelectorAll('.file-item').forEach((el,j)=>el.style.background=j===i?'rgba(0,217,255,0.3)':'rgba(255,255,255,0.05)');}
function play(){fetch('/play?index='+selIdx).then(r=>{document.getElementById('status').innerText='Воспроизведение...';});}
function stop(){fetch('/stop',{method:'POST'}).then(r=>{document.getElementById('status').innerText='Остановлено';});}
function setSpeed(v){fetch('/speed?value='+v);document.getElementById('speedVal').innerText=v.toFixed(1);}
function updateStatus(){fetch('/status').then(r=>r.json()).then(d=>{document.getElementById('status').innerText=d.status||'Ожидание';});}
loadFiles();setInterval(updateStatus,2000);
</script>
</body>
</html>)=====");
  
  server.send(200, "text/html", html);
}

// Обработчик статуса
void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["status"] = isPlaying ? "Воспроизведение..." : "Ожидание";
  doc["speed"] = playbackSpeed;
  doc["file"] = currentFileIndex >= 0 ? midiFiles[currentFileIndex].name : "";
  doc["sound"] = soundEnabled;
  doc["volume"] = volume;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  static File fsUploadFile;
  static String uploadFilename;

  if (upload.status == UPLOAD_FILE_START) {
    uploadFilename = "/midi_" + String(millis()) + ".mid";
    Serial.println("Uploading: " + uploadFilename);
    fsUploadFile = LittleFS.open(uploadFilename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
      Serial.println("Upload complete");

      // Парсинг MIDI файла
      MidiFile mf;
      mf.name = upload.filename;
      mf.totalDuration = 0;

      File file = LittleFS.open(uploadFilename, "r");
      processMIDIFile(file, mf);
      file.close();

      if (mf.notes.size() > 0) {
        midiFiles.push_back(mf);
        Serial.printf("Added file with %d notes\n", mf.notes.size());
      }

      server.send(200, "text/plain", "OK: " + upload.filename);
    }
  }
}

void handleFileList() {
  StaticJsonDocument<1024> doc;
  JsonArray files = doc.createNestedArray("files");
  
  for (int i = 0; i < midiFiles.size(); i++) {
    JsonObject f = files.createNestedObject();
    f["name"] = midiFiles[i].name;
    f["notes"] = midiFiles[i].notes.size();
    f["index"] = i;
  }
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handlePlay() {
  if (server.hasArg("index")) {
    currentFileIndex = server.arg("index").toInt();
    if (currentFileIndex >= 0 && currentFileIndex < midiFiles.size()) {
      isPlaying = true;
      playbackStartTime = millis();
      playbackSpeed = 1.0;

      websocket.broadcastTXT("PLAY:" + String(currentFileIndex));
      server.send(200, "text/plain", midiFiles[currentFileIndex].name);
      saveSettings();  // Сохраняем последний выбранный файл
      return;
    }
  }
  server.send(400, "text/plain", "Invalid index");
}

void handleStop() {
  isPlaying = false;
  FastLED.clear();
  FastLED.show();

  websocket.broadcastTXT("STOP");
  server.send(200, "text/plain", "Stopped");
}

void handleSpeed() {
  if (server.hasArg("value")) {
    playbackSpeed = server.arg("value").toFloat();
    if (playbackSpeed < 0.1) playbackSpeed = 0.1;
    if (playbackSpeed > 2.0) playbackSpeed = 2.0;

    websocket.broadcastTXT("SPEED:" + String(playbackSpeed));
    server.send(200, "text/plain", String(playbackSpeed));
    saveSettings();  // Сохраняем настройки
  } else {
    server.send(200, "text/plain", String(playbackSpeed));
  }
}

void handleMute() {
  toggleSound();
  websocket.broadcastTXT("MUTE:" + String(soundEnabled ? 1 : 0));
  server.send(200, "text/plain", soundEnabled ? "1" : "0");
}

// ==================== MIDI Парсинг ====================
void processMIDIFile(File file, MidiFile& mf) {
  // Упрощённый парсинг MIDI файла
  // Формат: читаем заголовок и треки
  
  if (!file) return;
  
  // Чтение заголовка MThd
  char header[4];
  file.readBytes(header, 4);
  
  if (strncmp(header, "MThd", 4) != 0) {
    Serial.println("Invalid MIDI file");
    return;
  }
  
  // Пропускаем длину заголовка (6 байт) и читаем формат
  file.seek(8);
  uint16_t format = file.read() << 8 | file.read();
  uint16_t tracks = file.read() << 8 | file.read();
  uint16_t division = file.read() << 8 | file.read();
  
  Serial.printf("MIDI Format: %d, Tracks: %d, Division: %d\n", format, tracks, division);
  
  // Чтение треков
  for (int t = 0; t < tracks; t++) {
    file.read(); // 'M'
    file.read(); // 'T'
    file.read(); // 'r'
    file.read(); // 'k'
    
    uint32_t trackLength = file.read() << 24 | file.read() << 16 | 
                           file.read() << 8 | file.read();
    
    uint32_t trackStart = file.position();
    uint32_t absoluteTime = 0;
    uint8_t lastStatus = 0;
    uint8_t runningStatus = 0;
    
    // Временное хранилище для нот
    struct PendingNote {
      uint8_t note;
      uint8_t velocity;
      uint32_t time;
    };
    std::vector<PendingNote> pendingNotes;
    
    while (file.position() < trackStart + trackLength) {
      // Чтение delta-time (variable length)
      uint32_t deltaTime = 0;
      byte b;
      do {
        b = file.read();
        deltaTime = (deltaTime << 7) | (b & 0x7F);
      } while (b & 0x80);
      
      // Конвертация в миллисекунды (при 120 BPM)
      uint32_t ms = (deltaTime * 500) / division;
      absoluteTime += ms;
      
      // Чтение статуса
      byte status = file.read();
      
      if (status & 0x80) {
        runningStatus = status;
      } else {
        status = runningStatus;
        file.seek(file.position() - 1);
      }
      
      uint8_t type = status & 0xF0;
      
      if (type == 0x90) { // Note On
        uint8_t note = file.read();
        uint8_t velocity = file.read();
        
        if (velocity > 0) {
          PendingNote pn;
          pn.note = note;
          pn.velocity = velocity;
          pn.time = absoluteTime;
          pendingNotes.push_back(pn);
        }
      } else if (type == 0x80) { // Note Off
        uint8_t note = file.read();
        uint8_t velocity = file.read();

        // Ищем соответствующую ноту и создаём событие
        for (int i = pendingNotes.size() - 1; i >= 0; i--) {
          if (pendingNotes[i].note == note) {
            NoteEvent ne;
            ne.note = pendingNotes[i].note;
            ne.velocity = pendingNotes[i].velocity;
            ne.startTime = pendingNotes[i].time;
            ne.duration = absoluteTime - pendingNotes[i].time;
            ne.endTime = absoluteTime;
            mf.notes.push_back(ne);
            pendingNotes.erase(pendingNotes.begin() + i);
            break;
          }
        }
      } else if (type == 0xC0) { // Program Change
        file.read(); // один байт данных
      } else if (type == 0xD0) { // Channel Aftertouch
        file.read();
      } else if (type == 0xB0 || type == 0xA0 || type == 0xE0) {
        file.read();
        file.read();
      } else if (status == 0xFF) { // Meta event
        uint8_t metaType = file.read();
        uint8_t metaLen = file.read();
        file.seek(file.position() + metaLen);
      } else if (status == 0xF0 || status == 0xF7) { // SysEx
        uint8_t sysexLen = 0;
        do {
          sysexLen = file.read();
          if (sysexLen & 0x80) {
            file.seek(file.position() + sysexLen - 0x80);
          }
        } while (sysexLen & 0x80);
      }
    }
  }
  
  // Сортировка нот по времени
  std::sort(mf.notes.begin(), mf.notes.end(), 
    [](const NoteEvent& a, const NoteEvent& b) {
      return a.startTime < b.startTime;
    });
  
  // Нахождение общей длительности
  if (mf.notes.size() > 0) {
    mf.totalDuration = mf.notes.back().startTime + mf.notes.back().duration;
  }
  
  Serial.printf("Parsed %d notes, total duration: %d ms\n", mf.notes.size(), mf.totalDuration);
}

void loadMidiFileList() {
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  while (file) {
    String name = file.name();
    if (name.endsWith(".mid") || name.endsWith(".midi")) {
      MidiFile mf;
      mf.name = name.substring(1); // убрать ведущий /
      mf.totalDuration = 0;
      
      processMIDIFile(file, mf);
      midiFiles.push_back(mf);
    }
    file = root.openNextFile();
  }
  
  Serial.printf("Loaded %d MIDI files\n", midiFiles.size());
}

// ==================== LED управление ====================
uint8_t noteToLED(uint8_t note) {
  // Маппинг нот на светодиоды (54 клавиши = 32 белых + 22 чёрных)
  // MIDI ноты: A0=21 до C5=72 (54 ноты)
  // LED: 0-53
  
  if (note < 21 || note > 74) return 255; // вне диапазона
  
  return note - 21;
}

void noteOn(uint8_t note, uint8_t velocity) {
  uint8_t led = noteToLED(note);
  if (led < NUM_LEDS) {
    // Цвет зависит от октавы
    uint8_t octave = (note - 21) / 12;
    
    // Белые клавиши (C, D, E, F, G, A, B)
    int noteInOctave = note % 12;
    bool isBlack = (noteInOctave == 1 || noteInOctave == 3 || 
                    noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);
    
    if (isBlack) {
      leds[led] = CRGB::Red;
    } else {
      // Разные цвета для разных октав
      switch (octave) {
        case 0: leds[led] = CRGB::Blue; break;
        case 1: leds[led] = CRGB::Green; break;
        case 2: leds[led] = CRGB::Yellow; break;
        case 3: leds[led] = CRGB::Orange; break;
        case 4: leds[led] = CRGB::White; break;
        default: leds[led] = CRGB::Cyan;
      }
    }
    
    // Яркость от velocity
    uint8_t brightness = map(velocity, 0, 127, 10, 255);
    leds[led].nscale8(brightness);
    
    FastLED.show();
  }
}

void noteOff(uint8_t note) {
  uint8_t led = noteToLED(note);
  if (led < NUM_LEDS) {
    leds[led] = CRGB::Black;
    FastLED.show();
  }
}

void updateLEDs() {
  if (currentFileIndex < 0 || currentFileIndex >= midiFiles.size()) {
    isPlaying = false;
    return;
  }

  MidiFile& mf = midiFiles[currentFileIndex];
  uint32_t currentTime = (millis() - playbackStartTime) * playbackSpeed;

  // Включаем ноты, которые должны звучать сейчас
  for (auto& note : mf.notes) {
    uint32_t adjustedStart = note.startTime / playbackSpeed;
    uint32_t adjustedEnd = note.endTime / playbackSpeed;

    // Проверка начала ноты
    if (currentTime >= adjustedStart && currentTime < adjustedStart + 50) {
      noteOn(note.note, note.velocity);
      if (soundEnabled) {
        // Запуск генерации звука для этой ноты
        ActiveNote an;
        an.note = note.note;
        an.endTime = adjustedEnd;
        activeNotes.push_back(an);
      }
    }

    // Проверка окончания ноты
    if (currentTime >= adjustedEnd && currentTime < adjustedEnd + 50) {
      noteOff(note.note);
      // Удаляем из активных
      for (auto it = activeNotes.begin(); it != activeNotes.end(); ++it) {
        if (it->note == note.note) {
          activeNotes.erase(it);
          break;
        }
      }
    }
  }

  // Проверка окончания
  if (currentTime >= mf.totalDuration / playbackSpeed) {
    isPlaying = false;
    FastLED.clear();
    FastLED.show();
    activeNotes.clear();
    websocket.broadcastTXT("END");
  }

  // Генерация звука для активных нот
  if (soundEnabled && activeNotes.size() > 0) {
    generateSound();
  }
}

// ==================== TFT Интерфейс ====================
#define BTN_PLAY_X   40
#define BTN_STOP_X   120
#define BTN_SEL_X    200
#define BTN_Y        260
#define BTN_W        70
#define BTN_H        50

int selectedFileIndex = 0;
int fileOffset = 0; // для карусели
#define MAX_VISIBLE_FILES 5

// Кнопки на экране
#define BTN_PLAY_X   20
#define BTN_STOP_X   95
#define BTN_SEL_X    170
#define BTN_MUTE_X   245
#define BTN_Y        260
#define BTN_W        65
#define BTN_H        50

void drawInterface() {
  tft.fillScreen(TFT_BLACK);

  // Заголовок
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("PIANO TRAINER", 80, 10);

  // Область для карусели файлов
  tft.drawRect(10, 50, 300, 150, TFT_WHITE);

  // Кнопки
  drawButtons();

  // Slider скорости
  tft.drawString("Speed:", 10, 220);
  tft.drawLine(60, 235, 280, 235, TFT_GRAY);

  drawFileCarousel();
}

void drawButtons() {
  // Play кнопка
  tft.fillRoundRect(BTN_PLAY_X, BTN_Y, BTN_W, BTN_H, 5, TFT_GREEN);
  tft.setTextColor(TFT_BLACK);
  tft.drawString("PLAY", BTN_PLAY_X + 5, BTN_Y + 18);

  // Stop кнопка
  tft.fillRoundRect(BTN_STOP_X, BTN_Y, BTN_W, BTN_H, 5, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("STOP", BTN_STOP_X + 5, BTN_Y + 18);

  // Select кнопка
  tft.fillRoundRect(BTN_SEL_X, BTN_Y, BTN_W, BTN_H, 5, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("SEL", BTN_SEL_X + 10, BTN_Y + 18);

  // MUTE кнопка
  if (soundEnabled) {
    tft.fillRoundRect(BTN_MUTE_X, BTN_Y, BTN_W, BTN_H, 5, TFT_ORANGE);
    tft.setTextColor(TFT_BLACK);
    tft.drawString("SND", BTN_MUTE_X + 8, BTN_Y + 18);
  } else {
    tft.fillRoundRect(BTN_MUTE_X, BTN_Y, BTN_W, BTN_H, 5, TFT_GRAY);
    tft.setTextColor(TFT_RED);
    tft.drawString("MUTE", BTN_MUTE_X + 5, BTN_Y + 18);
  }
}

void drawFileCarousel() {
  tft.fillRect(12, 52, 216, 146, TFT_BLACK);
  
  int startIdx = fileOffset;
  int endIdx = min(startIdx + MAX_VISIBLE_FILES, (int)midiFiles.size());
  
  for (int i = startIdx; i < endIdx; i++) {
    int y = 60 + (i - startIdx) * 28;
    
    if (i == selectedFileIndex) {
      tft.fillRect(14, y - 2, 212, 26, TFT_BLUE);
      tft.setTextColor(TFT_WHITE);
    } else {
      tft.setTextColor(TFT_GRAY);
    }
    
    String displayName = midiFiles[i].name;
    if (displayName.length() > 20) {
      displayName = displayName.substring(0, 18) + "..";
    }
    tft.drawString(displayName, 20, y);
  }
  
  // Индикаторы прокрутки
  if (fileOffset > 0) {
    tft.fillTriangle(110, 55, 105, 65, 115, 65, TFT_WHITE);
  }
  if (fileOffset + MAX_VISIBLE_FILES < midiFiles.size()) {
    tft.fillTriangle(110, 195, 105, 185, 115, 185, TFT_WHITE);
  }
}

void checkTouch() {
  // Упрощённая проверка тача (без реального драйвера XPT2046)
  // В реальной версии нужно добавить библиотеку XPT2046
  
  // Для симуляции проверяем веб-команды через WebSocket
}

void wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload);

    if (msg.startsWith("PLAY:")) {
      currentFileIndex = msg.substring(5).toInt();
      isPlaying = true;
      playbackStartTime = millis();
    } else if (msg == "STOP") {
      isPlaying = false;
      FastLED.clear();
      FastLED.show();
      activeNotes.clear();
    } else if (msg.startsWith("SPEED:")) {
      playbackSpeed = msg.substring(6).toFloat();
    } else if (msg.startsWith("SEL:")) {
      selectedFileIndex = msg.substring(4).toInt();
      drawFileCarousel();
    } else if (msg.startsWith("SCROLL:")) {
      int dir = msg.substring(7).toInt();
      fileOffset += dir;
      if (fileOffset < 0) fileOffset = 0;
      if (fileOffset >= midiFiles.size()) fileOffset = max(0, (int)midiFiles.size() - MAX_VISIBLE_FILES);
      drawFileCarousel();
    } else if (msg.startsWith("MUTE:")) {
      soundEnabled = msg.substring(5).toInt() == 1;
      drawButtons();
    }
  }
}

// ==================== Звук (I2S) ====================

// Частоты нот (4.5 октавы, A0=27.5Hz до C5=523.25Hz)
float noteToFreq(uint8_t note) {
  // A4 = 440Hz = MIDI нота 69
  return 440.0 * pow(2.0, (note - 69) / 12.0);
}

// Генерация синусоиды
void playTone(uint8_t note, uint8_t velocity) {
  if (!soundEnabled) return;

  float freq = noteToFreq(note);
  float amplitude = (velocity / 127.0) * volume * 32767.0;

  // Буфер для одного семпла (левый и правый каналы)
  int32_t sample[2];
  sample[0] = (int32_t)(amplitude * sin(2 * PI * freq * 0.0));  // Упрощённо
  sample[1] = sample[0];

  // Для простоты - просто отправляем тишину
  // Реальная генерация звука требует более сложного подхода
  // с использованием таблицы волн или DDS
}

void stopTone(uint8_t note) {
  // Остановка ноты (для простой реализации)
}

void toggleSound() {
  soundEnabled = !soundEnabled;
  drawButtons();
  saveSettings();  // Сохраняем настройки

  if (!soundEnabled) {
    i2s_zero_dma_buffer(I2S_PORT);
  }
}

// ==================== Сохранение настроек (Preferences) ====================

void saveSettings() {
  preferences.begin("piano", false);
  preferences.putFloat("speed", playbackSpeed);
  preferences.putBool("mute", !soundEnabled);
  preferences.putFloat("volume", volume);
  preferences.putInt("lastFile", currentFileIndex);
  preferences.end();
  Serial.println("Settings saved");
}

void loadSettings() {
  preferences.begin("piano", false);
  playbackSpeed = preferences.getFloat("speed", 1.0);
  soundEnabled = !preferences.getBool("mute", false);
  volume = preferences.getFloat("volume", 0.5f);
  currentFileIndex = preferences.getInt("lastFile", 0);
  preferences.end();
  
  Serial.printf("Settings loaded: speed=%.1f, mute=%d, volume=%.1f, lastFile=%d\n",
                playbackSpeed, !soundEnabled, volume, currentFileIndex);
}

// ==================== Генерация звука (упрощённая) ====================
// Для полноценной генерации звука нужен синтезатор

class SimpleSynth {
public:
  SimpleSynth() : phase(0) {}

  void setFreq(float freq) {
    phaseInc = freq * 2 * PI / SAMPLE_RATE;
  }

  int16_t nextSample() {
    phase += phaseInc;
    if (phase > 2 * PI) phase -= 2 * PI;
    return (int16_t)(sin(phase) * 32767);
  }

  void reset() {
    phase = 0;
  }

private:
  float phase = 0;
  float phaseInc = 0;
};

SimpleSynth synth;
uint32_t lastSampleTime = 0;

// Генерация звука для активных нот
#define AUDIO_BUFFER_SIZE 256
int16_t audioBuffer[AUDIO_BUFFER_SIZE * 2];  // Стерео

void generateSound() {
  if (activeNotes.size() == 0) return;

  uint32_t currentTime = millis();
  
  // Генерируем семплы для всех активных нот
  for (size_t i = 0; i < AUDIO_BUFFER_SIZE; i++) {
    float sample = 0;
    
    // Суммируем все активные ноты
    for (auto& an : activeNotes) {
      float freq = noteToFreq(an.note);
      float phase = 2 * PI * freq * (i / (float)SAMPLE_RATE);
      sample += sin(phase) * 0.3;  // Уменьшаем амплитуду для предотвращения клиппинга
    }
    
    // Ограничиваем диапазон
    if (sample > 1.0) sample = 1.0;
    if (sample < -1.0) sample = -1.0;
    
    int16_t sampleValue = (int16_t)(sample * volume * 32767);
    audioBuffer[i * 2] = sampleValue;      // Левый канал
    audioBuffer[i * 2 + 1] = sampleValue;  // Правый канал
  }

  // Отправляем в I2S
  size_t bytesWritten;
  i2s_write(I2S_PORT, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);
}
