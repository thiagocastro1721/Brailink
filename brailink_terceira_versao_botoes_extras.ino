#include <Arduino.h>
#include <KeyboardDevice.h>
#include <BleCompositeHID.h>
#include <KeyboardHIDCodes.h>

// Inicialização do Bluetooth HID
BleCompositeHID ble("TecladoBraille", "Maker", 100);
KeyboardDevice* kbd;

// Pinos dos botões
const int pins[] = {18, 4, 19, 32, 25, 15};
const int btnBack = 22, btnSpace = 21, btnEnter = 14;
const int btnUp = 16, btnDown = 26, btnLeft = 17, btnRight = 27;
const int btnVolumeUp = 13, btnVolumeDown = 12, btnReading = 33, btnPrint = 23;

// Estados e configuração de debounce
struct Button {
  bool state, lastState, pressed, changed;
  unsigned long lastTime;
};

Button btns[6];
bool lastSpace = HIGH, lastBack = HIGH, lastEnter = HIGH, lastUp = HIGH, lastDown = HIGH, lastLeft = HIGH, lastRight = HIGH, lastVolumeUp = HIGH, lastVolmeDown = HIGH, lastReading = HIGH, lastPrint = HIGH;
unsigned long timeSpace = 0, timeBack = 0, timeEnter = 0, timeUp = 0, timeDown = 0, timeLeft = 0, timeRight = 0, timeVolumeUp = 0, timeVolumeDown = 0, timeReading = 0, timePrint = 0;

// Mapeamento Braille (exemplo: letras a-z)
struct BrailleMap {
  uint8_t pattern[6];
  uint8_t key;
  bool needShift;
};

const BrailleMap letters[] = {
  {{1,0,0,0,0,0}, KEY_A, false, false, 0}, {{1,1,0,0,0,0}, KEY_B, false, false, 0},
  {{1,0,0,1,0,0}, KEY_C, false, false, 0}, {{1,0,0,1,1,0}, KEY_D, false, false, 0},
  {{1,0,0,0,1,0}, KEY_E, false, false, 0}, {{1,1,0,1,0,0}, KEY_F, false, false, 0},
  {{1,1,0,1,1,0}, KEY_G, false, false, 0}, {{1,1,0,0,1,0}, KEY_H, false, false, 0},
  {{0,1,0,1,0,0}, KEY_I, false, false, 0}, {{0,1,0,1,1,0}, KEY_J, false, false, 0},
  {{1,0,1,0,0,0}, KEY_K, false, false, 0}, {{1,1,1,0,0,0}, KEY_L, false, false, 0},
  {{1,0,1,1,0,0}, KEY_M, false, false, 0}, {{1,0,1,1,1,0}, KEY_N, false, false, 0},
  {{1,0,1,0,1,0}, KEY_O, false, false, 0}, {{1,1,1,1,0,0}, KEY_P, false, false, 0},
  {{1,1,1,1,1,0}, KEY_Q, false, false, 0}, {{1,1,1,0,1,0}, KEY_R, false, false, 0},
  {{0,1,1,1,0,0}, KEY_S, false, false, 0}, {{0,1,1,1,1,0}, KEY_T, false, false, 0},
  {{1,0,1,0,0,1}, KEY_U, false, false, 0}, {{1,1,1,0,0,1}, KEY_V, false, false, 0},
  {{0,1,0,1,1,1}, KEY_W, false, false, 0}, {{1,0,1,1,0,1}, KEY_X, false, false, 0},
  {{1,0,1,1,1,1}, KEY_Y, false, false, 0}, {{1,0,1,0,1,1}, KEY_Z, false, false, 0}
};

// Caracteres especiais (ç e acentos portugueses)
const BrailleMap specialChars[] = {
  // ç
  {{1,1,1,1,0,1}, 0, false, true, 231},    // (Alt+0231)
  
  // é  
  {{1,1,1,1,1,1}, 0, false, true, 233},    // (Alt+0233)
  
  // á
  {{1,1,1,0,1,1}, 0, false, true, 225},    // (Alt+0225)
  
  // è
  {{0,1,1,1,0,1}, 0, false, true, 232},    // (Alt+0232)
  
  // ú
  {{0,1,1,1,1,1}, 0, false, true, 250},    // (Alt+0250)
  
  // â
  {{1,0,0,0,0,1}, 0, false, true, 226},    // (Alt+0226)
  
  // ê
  {{1,1,0,0,0,1}, 0, false, true, 234},    // (Alt+0234)
  
  // ô
  {{1,0,0,1,1,1}, 0, false, true, 244},    // (Alt+0244)
  
  // @
  {{1,0,0,0,1,1}, KEY_2, true, false, 0},  // (Shift+2)
  
  // à
  {{1,1,0,1,0,1}, 0, false, true, 224},    // (Alt+0224)
  
  // õ 
  {{0,1,0,1,0,1}, 0, false, true, 245},    // (Alt+0245)

  // í 
  {{0,0,1,1,0,0}, 0, false, true, 237},    // (Alt+0237)

  // ó 
  {{0,0,1,1,0,1}, 0, false, true, 243},    // (Alt+0243)

  // ã 
  {{0,0,1,1,1,0}, 0, false, true, 227},    // (Alt+0227)
};

// Símbolos de pontuação
const BrailleMap symbols[] = {
  {{0,0,1,0,0,0}, KEY_DOT, false, false, 0},   // . ponto
  {{0,1,0,0,0,0}, KEY_COMMA, false, false, 0}, // , vírgula
  {{0,1,1,0,0,0}, 0, false, true, 59},         // ; ponto e vírgula
  {{0,1,0,0,1,0}, 0, false, true, 58},         // : dois pontos (Alt+58)
  {{0,1,0,0,0,1}, 0, false, true, 63},         // ? interrogação - Alt+63
  {{0,0,0,1,0,0}, KEY_MINUS, false, false, 0}, // . menos
  {{0,0,1,0,0,1}, KEY_MINUS, false, false, 0}, // - hífen
  {{0,1,1,0,1,0}, 0, false, true, 33}          // ! exclamação (Alt+33)
};

// Mapeamento de números (0-9)
const BrailleMap numbers[] = {
  {{1,0,0,0,0,0}, KEY_1, false, false, 0}, {{1,1,0,0,0,0}, KEY_2, false, false, 0},
  {{1,0,0,1,0,0}, KEY_3, false, false, 0}, {{1,0,0,1,1,0}, KEY_4, false, false, 0},
  {{1,0,0,0,1,0}, KEY_5, false, false, 0}, {{1,1,0,1,0,0}, KEY_6, false, false, 0},
  {{1,1,0,1,1,0}, KEY_7, false, false, 0}, {{1,1,0,0,1,0}, KEY_8, false, false, 0},
  {{0,1,0,1,0,0}, KEY_9, false, false, 0}, {{0,1,0,1,1,0}, KEY_0, false, false, 0}
};


// Funções auxiliares
bool debounce(int pin, bool &last, unsigned long &time) {
  bool current = digitalRead(pin);
  if (current != last) {
    time = millis();
    last = current;
  }
  return (millis() - time > 50 && current == LOW);
}

void updateButton(Button &btn, int pin) {
  bool reading = digitalRead(pin);
  if (reading != btn.lastState) {
    btn.lastTime = millis();
    btn.lastState = reading;
    btn.changed = false;
  }

  if (millis() - btn.lastTime > 50) {
    if (reading != btn.state) {
      btn.state = reading;
      btn.changed = true;
      btn.pressed = (btn.state == LOW);
    } else {
      btn.changed = false;
    }
  }
}

void sendKey(uint8_t key, bool withShift = false) {
  if (key == 0) return;
  if (withShift) {
    kbd->modifierKeyPress(KEY_MOD_LSHIFT);
    delay(10);
  }
  kbd->keyPress(key);
  delay(50);
  kbd->keyRelease(key);
  if (withShift) {
    delay(10);
    kbd->modifierKeyRelease(KEY_MOD_LSHIFT);
  }
  delay(25);
}

bool matchPattern(const uint8_t* pattern, const uint8_t* combo) {
  for (int i = 0; i < 6; i++) {
    if (pattern[i] != combo[i]) return false;
  }
  return true;
}

void processCombo(uint8_t* combo) {
  for (int i = 0; i < 26; i++) {
    if (matchPattern(letters[i].pattern, combo)) {
      sendKey(letters[i].key, letters[i].needShift);
      return;
    }
  }
}

// Setup
void setup() {
  Serial.begin(115200);
  kbd = new KeyboardDevice();
  ble.addDevice(kbd);
  ble.begin();

  for (int i = 0; i < 6; i++) {
    pinMode(pins[i], INPUT_PULLUP);
    btns[i] = {HIGH, HIGH, false, false, 0};
  }

  pinMode(btnBack, INPUT_PULLUP);
  pinMode(btnSpace, INPUT_PULLUP);
  pinMode(btnEnter, INPUT_PULLUP);
  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);
  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  pinMode(btnVolumeUp, INPUT_PULLUP);
  pinMode(btnVolumeDown, INPUT_PULLUP);
  pinMode(btnReading, INPUT_PULLUP);
  pinMode(btnPrint, INPUT_PULLUP); 
}

// Loop
void loop() {
  static uint8_t combo[6] = {0};
  static unsigned long startTime = 0;
  static bool collecting = false;

  for (int i = 0; i < 6; i++) {
    updateButton(btns[i], pins[i]);

    if (btns[i].pressed && !collecting) {
      collecting = true;
      startTime = millis();
      memset(combo, 0, 6);
    }

    if (btns[i].pressed) {
      combo[i] = 1;
    }
  }

  if (collecting) {
    bool anyPressed = false;
    for (int i = 0; i < 6; i++) {
      if (btns[i].pressed) {
        anyPressed = true;
        break;
      }
    }

    if (!anyPressed && millis() - startTime > 300) {
      collecting = false;
      processCombo(combo);
      memset(combo, 0, 6);
    }
  }

  if (debounce(btnSpace, lastSpace, timeSpace)) sendKey(KEY_SPACE);
  if (debounce(btnBack, lastBack, timeBack)) sendKey(KEY_BACKSPACE);
  if (debounce(btnEnter, lastEnter, timeEnter)) sendKey(KEY_ENTER);
  if (debounce(btnUp, lastUp, timeUp)) sendKey(KEY_UP_ARROW);
  if (debounce(btnDown, lastDown, timeDown)) sendKey(KEY_DOWN_ARROW);
  if (debounce(btnLeft, lastLeft, timeLeft)) sendKey(KEY_LEFT_ARROW);
  if (debounce(btnRight, lastRight, timeRight)) sendKey(KEY_RIGHT_ARROW);
  if (debounce(btnVolumeUp, lastVolumeUp, timeVolumeUp)) sendKey(KEY_VOLUME_UP);
  if (debounce(btnVolumeDown, lastVolumeDown, timeVolumeDown)) sendKey(KEY_VOLUME_DOWN);
  if (debounce(btnReading, lastReading, timeReading)) sendKey(KEY_READING);
  if (debounce(btnPrint, lastPrint, timePrint)) sendKey(KEY_PRINT);


  delay(5);
}
