#include <Arduino.h>
#include <KeyboardDevice.h>
#include <BleCompositeHID.h>
#include <KeyboardHIDCodes.h>

BleCompositeHID ble("Teclado Brailink", "Maker", 100);
KeyboardDevice* kbd;

// Pinos dos botões originais
const int pins[] = {18, 5, 17, 19, 21, 22}; // P1-P6 (TX2, D5, D18, D19, D21, D22)
const int btnBack = 12, btnSpace = 27, btnEnter = 16;

// Novos pinos dos botões de controle
const int btnVolumeUp = 32;
const int btnVolumeDown = 33;
const int btnReading = 26;    // Narrador
const int btnPrint = 25;
const int btnDirUp = 14;      // Novo: Direcional cima
const int btnDirDown = 23;    // Novo: Direcional baixo  
const int btnDirLeft = 13;    // Novo: Direcional esquerda
const int btnDirRight = 4;    // Novo: Direcional direita

// Variáveis para deep sleep e controle de botão longo
unsigned long volumeDownPressTime = 0;
bool volumeDownLongPress = false;
const unsigned long LONG_PRESS_TIME = 3000; // 3 segundos

// Controle de estado Bluetooth - MELHORADO
bool wasConnected = false;
bool initialConnectionMade = false;
unsigned long lastConnectionCheck = 0;
const unsigned long CONNECTION_CHECK_INTERVAL = 1000;
unsigned long disconnectionTime = 0;
const unsigned long RECONNECTION_TIMEOUT = 10000; // 10 segundos para reiniciar
bool disconnectionDetected = false;
int reconnectionAttempts = 0;
const int MAX_RECONNECTION_ATTEMPTS = 1; // Máximo 1 tentativa

//Variaveis para inatividade ou desconexao prolongada
unsigned long lastActivityTime = 0;
unsigned long lastKeyPressTime = 0;
//const unsigned long INACTIVITY_SLEEP_TIMEOUT = 120000; // 2 minutos sem atividade
const unsigned long INACTIVITY_SLEEP_TIMEOUT = 3600000; // 1 hora sem atividade
const unsigned long DISCONNECTION_SLEEP_TIMEOUT = 60000; // 1 minuto sem conexão
bool autoSleepEnabled = true;
bool lastDirUp = HIGH, lastDirDown = HIGH, lastDirLeft = HIGH, lastDirRight = HIGH;
unsigned long timeDirUp = 0, timeDirDown = 0, timeDirLeft = 0, timeDirRight = 0;

bool justWokeUp = false;

// Estrutura para mapeamento Braille
struct BrailleMap {
  uint8_t pattern[6];
  uint8_t key;
  bool needShift;
  bool isSpecial; // Para caracteres especiais como ç, acentos, etc.
  uint16_t altCode; // Código Alt para caracteres especiais
};

// Mapeamento de letras (a-z)
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

// Indicadores especiais
const uint8_t CAPS_IND[] = {0,0,0,1,0,1};     // maiúscula (pontos 4 e 6)
const uint8_t NUM_IND[] = {0,0,1,1,1,1};      // números (pontos 3,4,5,6)

// Estados
enum Mode { NORMAL, UPPERCASE, NUMBER };
Mode currentMode = NORMAL;

// Debounce
struct Button {
  bool state, lastState, pressed, changed;
  unsigned long lastTime;
};

Button btns[6];
bool lastSpace = HIGH, lastBack = HIGH, lastEnter = HIGH;
bool lastVolumeUp = HIGH, lastVolumeDown = HIGH, lastReading = HIGH, lastPrint = HIGH;
unsigned long timeSpace = 0, timeBack = 0, timeEnter = 0;
unsigned long timeVolumeUp = 0, timeVolumeDown = 0, timeReading = 0, timePrint = 0;

// Buffer de combinação
uint8_t combo[6] = {0};
unsigned long startTime = 0;
bool collecting = false;
const unsigned long TIMEOUT = 300;

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

bool matchPattern(const uint8_t* pattern, const uint8_t* combo) {
  for (int i = 0; i < 6; i++) {
    if (pattern[i] != combo[i]) return false;
  }
  return true;
}

void sendKey(uint8_t key, bool withShift = false) {
  if (key == 0) return;
  updateActivity();
  
  if (withShift) {
    kbd->modifierKeyPress(KEY_MOD_LSHIFT);
    delay(10);
    kbd->keyPress(key);
    delay(50);
    kbd->keyRelease(key);
    delay(10);
    kbd->modifierKeyRelease(KEY_MOD_LSHIFT);
    delay(25);
  } else {
    kbd->keyPress(key);
    delay(50);
    kbd->keyRelease(key);
    delay(25);
  }
}

//Função para diminuir volume
void decreaseVolume() {
  if (volumeDownLongPress) {
    enterDeepSleep();
    return;
  }
  else{
    //sendKey(KEY_VOLUMEDOWN);

    kbd->keyPress(KEY_VOLUMEDOWN);
    delay(50);
    kbd->keyRelease(KEY_VOLUMEDOWN);
    delay(10);
  }
  Serial.println("Volume down pressionado (funcionalidade de áudio removida)");
}

//Função para aumentar volume
void increaseVolume() {
  sendKey(KEY_VOLUMEUP);
  Serial.println("Volume up pressionado");
}


// Função para ativar o Narrador do Windows
void activateReading() {
  
  // Atalho Win+Ctrl+Enter para ativar/desativar o Narrador do Windows
  kbd->modifierKeyPress(KEY_MOD_LMETA | KEY_MOD_LCTRL);
  delay(10);
  kbd->keyPress(KEY_ENTER);
  delay(50);
  kbd->keyRelease(KEY_ENTER);
  delay(10);
  kbd->modifierKeyRelease(KEY_MOD_LMETA | KEY_MOD_LCTRL);
  
  Serial.println("Narrador do Windows ativado/desativado: Win+Ctrl+Enter");
}


// Função para ativar impressão
void activatePrint() {
  
  // Atalho Ctrl+P para abrir diálogo de impressão
  kbd->modifierKeyPress(KEY_MOD_LCTRL);
  delay(10);
  kbd->keyPress(KEY_P);
  delay(50);
  kbd->keyRelease(KEY_P);
  delay(10);
  kbd->modifierKeyRelease(KEY_MOD_LCTRL);
  
  Serial.println("Diálogo de impressão ativado: Ctrl+P");
}

// Funções das teclas direcionais
void pressDirectionalUp() {
  sendKey(KEY_UP);
  Serial.println("Direcional cima pressionado");
}

void pressDirectionalDown() {
  sendKey(KEY_DOWN);
  Serial.println("Direcional baixo pressionado");
}

void pressDirectionalLeft() {
  sendKey(KEY_LEFT);
  Serial.println("Direcional esquerda pressionado");
}

void pressDirectionalRight() {
  sendKey(KEY_RIGHT);
  Serial.println("Direcional direita pressionado");
}

void sendAltCode(uint16_t altCode, bool uppercase = false) {
  Serial.printf("Enviando Alt+%d...\n", altCode);
  
  // Se for para maiúscula, ajustar código Alt
  if (uppercase) {
    switch(altCode) {
      case 231: altCode = 199; break; // ç -> Ç
      case 233: altCode = 201; break; // é -> É  
      case 225: altCode = 193; break; // á -> Á
      case 232: altCode = 200; break; // è -> È
      case 250: altCode = 218; break; // ú -> Ú
      case 226: altCode = 194; break; // â -> Â
      case 234: altCode = 202; break; // ê -> Ê
      case 244: altCode = 212; break; // ô -> Ô
      case 224: altCode = 192; break; // à -> À
      case 245: altCode = 213; break; // õ -> Õ
    }
    Serial.printf("Versão maiúscula: Alt+%d\n", altCode);
  }
  
  // Pressionar Alt esquerdo
  kbd->modifierKeyPress(KEY_MOD_LALT);
  delay(100);
  
  // Converter número para sequência de dígitos
  String codeStr = String(altCode);
  if (codeStr.length() < 4) {
    codeStr = "0" + codeStr; // Adicionar 0 inicial se necessário
  }
  
  // Enviar cada dígito
  for (int i = 0; i < codeStr.length(); i++) {
    char digit = codeStr.charAt(i);
    uint8_t keyCode;
    
    switch(digit) {
      case '0': keyCode = KEY_KP0; break;
      case '1': keyCode = KEY_KP1; break;
      case '2': keyCode = KEY_KP2; break;
      case '3': keyCode = KEY_KP3; break;
      case '4': keyCode = KEY_KP4; break;
      case '5': keyCode = KEY_KP5; break;
      case '6': keyCode = KEY_KP6; break;
      case '7': keyCode = KEY_KP7; break;
      case '8': keyCode = KEY_KP8; break;
      case '9': keyCode = KEY_KP9; break;
      default: continue;
    }
    
    kbd->keyPress(keyCode);
    delay(50);
    kbd->keyRelease(keyCode);
    delay(50);
  }
  
  // Soltar Alt
  kbd->modifierKeyRelease(KEY_MOD_LALT);
  delay(200);
  
  Serial.println("Caractere especial enviado!");
}

void processCombo() {
  updateActivity();
  // Debug: mostrar o padrão detectado
  Serial.print("Padrão detectado: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(combo[i]);
    if (i < 5) Serial.print(",");
  }
  Serial.println();
  
  // Verificar indicador de maiúscula (pontos 4 e 6)
  if (matchPattern(CAPS_IND, combo)) {
    currentMode = UPPERCASE;
    Serial.println("✓ MODO MAIÚSCULA ATIVADO!");
    return;
  }
  
  // Verificar indicador numérico (pontos 3,4,5,6)
  if (matchPattern(NUM_IND, combo)) {
    if (currentMode == NUMBER) {
      currentMode = NORMAL;
      Serial.println("Saindo do modo numérico");
    } else {
      currentMode = NUMBER;
      Serial.println("Modo: Números ativado");
    }
    return;
  }
  
  // Processar números
  if (currentMode == NUMBER) {
    for (int i = 0; i < 10; i++) {
      if (matchPattern(numbers[i].pattern, combo)) {
        sendKey(numbers[i].key);
        Serial.printf("Número enviado: %d\n", i == 9 ? 0 : i + 1);
        return;
      }
    }
    // Se não for um número válido, sair do modo numérico
    Serial.println("Padrão inválido no modo numérico, voltando ao normal");
    currentMode = NORMAL;
  }
  
  // Processar caracteres especiais
  for (int i = 0; i < sizeof(specialChars)/sizeof(specialChars[0]); i++) {
    if (matchPattern(specialChars[i].pattern, combo)) {
      bool useUppercase = (currentMode == UPPERCASE);
      
      if (specialChars[i].isSpecial && specialChars[i].altCode > 0) {
        sendAltCode(specialChars[i].altCode, useUppercase);
        Serial.printf("Caractere especial enviado (Alt+%d)\n", specialChars[i].altCode);
      } else {
        sendKey(specialChars[i].key, specialChars[i].needShift || useUppercase);
        Serial.println("Caractere especial enviado");
      }
      
      // Sair do modo maiúscula única após usar
      if (currentMode == UPPERCASE) {
        currentMode = NORMAL;
        Serial.println("Voltando ao modo normal após maiúscula");
      }
      return;
    }
  }
  
  // Processar símbolos
  for (int i = 0; i < sizeof(symbols)/sizeof(symbols[0]); i++) {
    if (matchPattern(symbols[i].pattern, combo)) {
      if (symbols[i].isSpecial && symbols[i].altCode > 0) {
        sendAltCode(symbols[i].altCode);
        Serial.printf("Símbolo especial enviado (Alt+%d)\n", symbols[i].altCode);
      } else {
        sendKey(symbols[i].key, symbols[i].needShift);
        Serial.println("Símbolo enviado");
      }
      return;
    }
  }
  
  // Processar letras
  for (int i = 0; i < 26; i++) {
    if (matchPattern(letters[i].pattern, combo)) {
      bool useShift = (currentMode == UPPERCASE);
      
      Serial.printf("Processando letra %c, modo atual: %d, usar shift: %s\n", 
                    'a' + i, currentMode, useShift ? "SIM" : "NÃO");
      
      sendKey(letters[i].key, useShift);
      
      Serial.printf("Letra enviada: %c%s\n", 'a' + i, useShift ? " (maiúscula)" : "");
      
      // Sair do modo numérico se uma letra for digitada
      if (currentMode == NUMBER) {
        currentMode = NORMAL;
        Serial.println("Letra digitada, saindo do modo numérico");
      }
      
      // Sair do modo maiúscula única após usar
      if (currentMode == UPPERCASE) {
        currentMode = NORMAL;
        Serial.println("Voltando ao modo normal após maiúscula");
      }
      return;
    }
  }
  
  Serial.println("✗ Nenhum padrão reconhecido");
}

void enterDeepSleep() {
  Serial.println("Entrando em Deep Sleep...");
  
  // Configurar wakeup no botão volume+ (pino 13)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 0); // Volume+ agora é D32
  
  // Entrar em deep sleep
  esp_deep_sleep_start();
}

void checkBluetoothConnection() {
  bool currentlyConnected = ble.isConnected();
  
  if (currentlyConnected != wasConnected) {
    if (currentlyConnected) {
      Serial.println("=== BLUETOOTH CONECTADO ===");
      disconnectionDetected = false;
      reconnectionAttempts = 0;
      updateActivity(); // <- ADICIONAR ESTA LINHA
      
      if (initialConnectionMade) {
        delay(500);
      } else {
        delay(2000);
        initialConnectionMade = true;
      }
    } else {
      // Acabou de desconectar
      Serial.println("=== BLUETOOTH DESCONECTADO ===");
      disconnectionTime = millis();
      disconnectionDetected = true;
    }
    wasConnected = currentlyConnected;
  }
  
  // Se desconectado por muito tempo, verificar tentativas
  if (disconnectionDetected && !currentlyConnected) {
    if (millis() - disconnectionTime > RECONNECTION_TIMEOUT) {
      
      if (reconnectionAttempts < MAX_RECONNECTION_ATTEMPTS) {
        // Ainda pode tentar reconectar
        reconnectionAttempts++;
        Serial.printf("Tentativa de reconexão %d/%d. Reiniciando ESP32...\n", 
                      reconnectionAttempts, MAX_RECONNECTION_ATTEMPTS);
        delay(1000);
        ESP.restart();
      } else {
        // Máximo de tentativas atingido
        Serial.println("Máximo de tentativas de reconexão atingido.");
        Serial.println("Parando tentativas automáticas de reconexão.");
        disconnectionDetected = false; // Para de verificar
        
      }
    }
  }
}

void printBluetoothStatus() {
  if (ble.isConnected()) {
    Serial.println("Status BT: CONECTADO");
  } else {
    Serial.println("Status BT: AGUARDANDO CONEXÃO");
  }
}

void processControlButtons() {
  // Verificar botão de leitura
  if (debounce(btnReading, lastReading, timeReading)) {
    delay(150);
    return;
  }
  
  // Verificar botão de impressão  
  if (debounce(btnPrint, lastPrint, timePrint)) {
    activatePrint();
    delay(150);
    return;
  }

  // Verificar botão volume down com suporte a long press
  bool currentVolumeDown = digitalRead(btnVolumeDown);
  if (currentVolumeDown != lastVolumeDown) {
    if (currentVolumeDown == LOW) {
      // Botão pressionado
      volumeDownPressTime = millis();
      volumeDownLongPress = false;
    } else {
      // Botão solto
      if (!volumeDownLongPress && (millis() - volumeDownPressTime < LONG_PRESS_TIME)) {
        // Press curto - diminuir volume
        decreaseVolume();
      }
    }
    timeVolumeDown = millis();
    lastVolumeDown = currentVolumeDown;
  }

  // Verificar botão volume up - ignorar primeira pressão após acordar
  if (debounce(btnVolumeUp, lastVolumeUp, timeVolumeUp)) {
    if (justWokeUp) {
      // Ignorar primeira pressão após acordar (foi usada para acordar)
      justWokeUp = false;
      Serial.println("Primeira pressão do volume+ ignorada (usado para acordar)");
    } else {
      // Funcionamento normal
      increaseVolume(); 
    }
    delay(150);
    return;
  }

  // Verificar se é long press
  if (currentVolumeDown == LOW && !volumeDownLongPress && 
      (millis() - volumeDownPressTime >= LONG_PRESS_TIME)) {
    volumeDownLongPress = true;
    Serial.println("Long press detectado - preparando para deep sleep");
    decreaseVolume(); // Chamará enterDeepSleep()
  }
}

//Função para atualizar atividade
void updateActivity() {
  lastActivityTime = millis();
  lastKeyPressTime = millis();
  Serial.println("Atividade detectada - timer resetado");
}

//Funcao para verificar se deve entrar em sleep:
void checkAutoSleep() {
  if (!autoSleepEnabled) return;
  
  unsigned long currentTime = millis();
  bool shouldSleep = false;
  String reason = "";
  
  // Cenário 1: Desconectado e sem tentativas de reconexão restantes
  if (!ble.isConnected() && reconnectionAttempts >= MAX_RECONNECTION_ATTEMPTS) {
    if (currentTime - disconnectionTime > DISCONNECTION_SLEEP_TIMEOUT) {
      shouldSleep = true;
      reason = "sem conexão por muito tempo";
    }
  }
  
  // Cenário 2: Conectado mas sem atividade por muito tempo
  else if (ble.isConnected() && lastActivityTime > 0) {
    if (currentTime - lastActivityTime > INACTIVITY_SLEEP_TIMEOUT) {
      shouldSleep = true;
      reason = "inatividade prolongada";
    }
  }
  
  // Cenário 3: Nunca conectou e passou muito tempo
  else if (!initialConnectionMade) {
    if (currentTime > DISCONNECTION_SLEEP_TIMEOUT) {
      shouldSleep = true;
      reason = "tempo limite para primeira conexão";
    }
  }
  
  if (shouldSleep) {
    Serial.printf("Entrando em auto sleep por: %s\n", reason.c_str());
    //myDFPlayer.playFolder(3, 6); // Áudio de auto sleep (se tiver)
    delay(1000);
    enterDeepSleep();
  }
}

// Para desabilitar auto sleep temporariamente (adicionar função):
void toggleAutoSleep() {
  autoSleepEnabled = !autoSleepEnabled;
  Serial.printf("Auto sleep %s\n", autoSleepEnabled ? "habilitado" : "desabilitado");
}

// Para usar com combinação de botões específica, adicionar em processCombo():
// Exemplo: padrão específico para toggle auto sleep
// if (matchPattern({0,0,0,0,1,1}, combo)) { // Pontos 5 e 6
//   toggleAutoSleep();
//   return;
// }


void setup() {
  Serial.begin(115200);

  // Verificar causa do wakeup
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool wokeFromSleep = false;
  
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Acordado por botão externo (Volume+)");
      wokeFromSleep = true;
      break;
    default:
      Serial.println("Inicialização normal");
      break;
  }

  // Marcar se acabou de acordar
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    justWokeUp = true;
  }
  
  // Aguardar inicialização
  delay(1000);
  
  // Inicializar Bluetooth
  kbd = new KeyboardDevice();
  ble.addDevice(kbd);
  ble.begin();
  
  // Inicializar estado de conexão
  wasConnected = false;
  initialConnectionMade = false;
  //needsReconnection = false;

  // Mostrar informações de conexão
  Serial.println("=== BLUETOOTH INICIADO ===");
  Serial.println("Nome do dispositivo: Teclado Brailink");
  Serial.println("Status: Aguardando primeira conexão...");
  
  // Configurar pinos originais
  for (int i = 0; i < 6; i++) {
    pinMode(pins[i], INPUT_PULLUP);
    btns[i] = {HIGH, HIGH, false, false, 0};
  }
  
  pinMode(btnBack, INPUT_PULLUP);
  pinMode(btnSpace, INPUT_PULLUP);
  pinMode(btnEnter, INPUT_PULLUP);
  
  // Configurar novos pinos de controle
  pinMode(btnVolumeUp, INPUT_PULLUP);
  pinMode(btnVolumeDown, INPUT_PULLUP);
  pinMode(btnReading, INPUT_PULLUP);
  pinMode(btnPrint, INPUT_PULLUP);

  // Configurar pinos das teclas direcionais
  pinMode(btnDirUp, INPUT_PULLUP);
  pinMode(btnDirDown, INPUT_PULLUP);
  pinMode(btnDirLeft, INPUT_PULLUP);
  pinMode(btnDirRight, INPUT_PULLUP);
  
  // Garantir que inicia no modo normal
  currentMode = NORMAL;
  
  Serial.println("=== Teclado Brailink iniciado ===");
  Serial.println("Modo inicial: NORMAL (minúsculas)");
  Serial.println("Caracteres especiais suportados:");
  Serial.println("ç/Ç, é/É, á/Á, è/È, ú/Ú, â/Â, ê/Ê, ô/Ô, @, à/À, õ/Õ");
  Serial.println("Símbolos: , ; : ? (Alt+63) . ! (Alt+33)");
  Serial.println("Controles de áudio:");
  Serial.println("D32=Vol+, D33=Vol-, D26=Narrador, D25=Impressão");
  Serial.println("Direcionais: D14=↑, D23=↓, D13=←, D4=→");
  Serial.printf("Estado inicial - currentMode: %d\n", currentMode);

  lastActivityTime = millis(); // Inicializar timer de atividade
  lastKeyPressTime = millis();
  
  Serial.println("=== AUTO SLEEP CONFIGURADO ===");
  Serial.printf("Sleep por inatividade: %lu minutos\n", INACTIVITY_SLEEP_TIMEOUT / 60000);
  Serial.printf("Sleep por desconexão: %lu minutos\n", DISCONNECTION_SLEEP_TIMEOUT / 60000);
  Serial.println("Pressione Vol- por 3s para sleep manual");
}

void loop() {
  // Verificar mudanças na conexão Bluetooth periodicamente
  if (millis() - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
    checkBluetoothConnection();
    lastConnectionCheck = millis();
  }

  checkAutoSleep();
  
  // Se não está conectado, processar apenas controles de áudio
  if (!ble.isConnected()) {
    // Mostrar status incluindo tempo para sleep
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 10000) {
      printBluetoothStatus();
      
      if (disconnectionDetected && reconnectionAttempts < MAX_RECONNECTION_ATTEMPTS) {
        unsigned long timeLeft = RECONNECTION_TIMEOUT - (millis() - disconnectionTime);
        Serial.printf("Tentativa %d/%d - Reinicializando em %lu segundos...\n", 
                      reconnectionAttempts + 1, MAX_RECONNECTION_ATTEMPTS, timeLeft / 1000);
      } else if (reconnectionAttempts >= MAX_RECONNECTION_ATTEMPTS) {
        unsigned long sleepTimeLeft = DISCONNECTION_SLEEP_TIMEOUT - (millis() - disconnectionTime);
        if (sleepTimeLeft > 0) {
          Serial.printf("Auto sleep em %lu segundos...\n", sleepTimeLeft / 1000);
        }
      }
      lastStatusPrint = millis();
    }
    
    processControlButtons();
    delay(500);
    return;
  }
  
  // ADICIONAR: Mostrar tempo para sleep por inatividade quando conectado
  static unsigned long lastInactivityPrint = 0;
  if (millis() - lastInactivityPrint > 30000 && lastActivityTime > 0) { // A cada 30s
    unsigned long timeSinceActivity = millis() - lastActivityTime;
    if (timeSinceActivity > INACTIVITY_SLEEP_TIMEOUT / 2) { // Avisar quando passar da metade
      unsigned long timeLeft = INACTIVITY_SLEEP_TIMEOUT - timeSinceActivity;
      Serial.printf("Auto sleep por inatividade em %lu segundos...\n", timeLeft / 1000);
    }
    lastInactivityPrint = millis();
  }
  
  // Verificar botões de impressão e leitura
  if (debounce(btnReading, lastReading, timeReading)) {
    activateReading();
    delay(150);
    return;
  }
  
  if (debounce(btnPrint, lastPrint, timePrint)) {
    activatePrint();
    delay(150);
    return;
  }


  // Verificar botão volume down com suporte a long press
  bool currentVolumeDown = digitalRead(btnVolumeDown);
  if (currentVolumeDown != lastVolumeDown) {
    if (currentVolumeDown == LOW) {
      // Botão pressionado
      volumeDownPressTime = millis();
      volumeDownLongPress = false;
    } else {
      // Botão solto
      if (!volumeDownLongPress && (millis() - volumeDownPressTime < LONG_PRESS_TIME)) {
        // Press curto - diminuir volume
        decreaseVolume();
      }
    }
    timeVolumeDown = millis();
    lastVolumeDown = currentVolumeDown;
  }

  // Verificar botão volume up - ignorar primeira pressão após acordar
  if (debounce(btnVolumeUp, lastVolumeUp, timeVolumeUp)) {
    if (justWokeUp) {
      // Ignorar primeira pressão após acordar (foi usada para acordar)
      justWokeUp = false;
      Serial.println("Primeira pressão do volume+ ignorada (usado para acordar)");
    } else {
      // Funcionamento normal
      increaseVolume();
    }
    delay(150);
    return;
  }

  // Verificar teclas direcionais
  if (debounce(btnDirUp, lastDirUp, timeDirUp)) {
    pressDirectionalUp();
    delay(150);
    return;
  }

  if (debounce(btnDirDown, lastDirDown, timeDirDown)) {
    pressDirectionalDown();
    delay(150);
    return;
  }

  if (debounce(btnDirLeft, lastDirLeft, timeDirLeft)) {
    pressDirectionalLeft();
    delay(150);
    return;
  }

  if (debounce(btnDirRight, lastDirRight, timeDirRight)) {
    pressDirectionalRight();
    delay(150);
    return;
  }

  // Verificar se é long press
  if (currentVolumeDown == LOW && !volumeDownLongPress && 
      (millis() - volumeDownPressTime >= LONG_PRESS_TIME)) {
    volumeDownLongPress = true;
    Serial.println("Long press detectado - preparando para deep sleep");
    decreaseVolume(); // Chamará enterDeepSleep()
  }
  
  // Teclas especiais originais
  if (debounce(btnSpace, lastSpace, timeSpace)) {
    updateActivity();
    sendKey(KEY_SPACE);
    // Sair do modo numérico após espaço
    if (currentMode == NUMBER) {
      currentMode = NORMAL;
      Serial.println("Espaço pressionado, saindo do modo numérico");
    }
    delay(150);
    return;
  }
  if (debounce(btnBack, lastBack, timeBack)) {
    updateActivity();
    sendKey(KEY_BACKSPACE);
    delay(150);
    return;
  }
  if (debounce(btnEnter, lastEnter, timeEnter)) {
    updateActivity();
    sendKey(KEY_ENTER);
    delay(150);
    return;
  }
  
  // Processar botões Braille
  bool anyPressed = false, anyChanged = false;
  
  for (int i = 0; i < 6; i++) {
    updateButton(btns[i], pins[i]);
    
    if (btns[i].pressed) anyPressed = true;
    if (btns[i].changed) {
      anyChanged = true;
      
      if (btns[i].pressed && !collecting) {
        collecting = true;
        startTime = millis();
        memset(combo, 0, 6);
      }
      
      if (btns[i].pressed) {
        combo[i] = 1;
      }
    }
  }
  
  // Finalizar combinação
  if (collecting && !anyPressed) {
    if (millis() - startTime > TIMEOUT) {
      collecting = false;
      Serial.println("=== FINALIZANDO COMBINAÇÃO ===");
      Serial.print("Botões pressionados durante a combinação: ");
      for (int i = 0; i < 6; i++) {
        Serial.printf("P%d=%d ", i+1, combo[i]);
      }
      Serial.println();
      Serial.printf("Modo atual antes de processar: %d\n", currentMode);
      processCombo();
      memset(combo, 0, 6);
    }
  }
  
  // Reiniciar timer se botão pressionado
  if (collecting && anyChanged && anyPressed) {
    startTime = millis();
  }
  
  delay(5);
}
