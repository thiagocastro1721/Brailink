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

// Estados e configuração de debounce
struct Button {
  bool state, lastState, pressed, changed;
  unsigned long lastTime;
};

Button btns[6];
bool lastSpace = HIGH, lastBack = HIGH, lastEnter = HIGH, lastUp = HIGH, lastDown = HIGH, lastLeft = HIGH, lastRight = HIGH;
unsigned long timeSpace = 0, timeBack = 0, timeEnter = 0 timeUp = 0, timeDown = 0, timeLeft = 0, timeRight = 0;

// Mapeamento Braille (exemplo: letras a-z)
struct BrailleMap {
  uint8_t pattern[6];
  uint8_t key;
  bool needShift;
};

const BrailleMap letters[] = {
  {{1,0,0,0,0,0}, KEY_A, false}, {{1,1,0,0,0,0}, KEY_B, false},
  // ... continuar com os demais
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


  delay(5);
}
