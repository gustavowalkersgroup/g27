/*
 * G27 Pedals USB - Firmware HID Joystick
 * Placa: Arduino Pro Micro (ATmega32U4, 5V, 16MHz)
 *
 * Dependência obrigatória (gratuita):
 *   Biblioteca "Joystick" de Matthew Heironimus
 *   Instale via: Sketch → Incluir Biblioteca → Gerenciar Bibliotecas
 *   Pesquise: "Joystick" por "Matthew Heironimus"
 *   URL: https://github.com/MHeironimus/ArduinoJoystickLibrary
 *
 * O Windows reconhecerá o dispositivo como "G27 Pedals USB"
 * com três eixos analógicos:
 *   X Axis → Acelerador (A0)
 *   Y Axis → Freio      (A1)
 *   Z Axis → Embreagem  (A2)
 *
 * Ligações físicas:
 *   Fio 5V  do conector dos pedais → VCC do Arduino
 *   Fio GND do conector dos pedais → GND do Arduino
 *   Sinal Acelerador               → A0
 *   Sinal Freio                    → A1
 *   Sinal Embreagem                → A2
 *
 * Como compilar e gravar:
 *   1. Instale a biblioteca Joystick (instruções acima).
 *   2. Em Ferramentas → Placa, selecione:
 *      "Arduino Leonardo" OU "SparkFun Pro Micro 5V/16MHz"
 *      (o Pro Micro usa o mesmo chip ATmega32U4 do Leonardo).
 *   3. Em Ferramentas → Porta, selecione a porta COM do Pro Micro.
 *   4. Clique em Upload (→).
 *   5. Abra joy.cpl (Win+R → joy.cpl) e confirme "G27 Pedals USB".
 *
 * Como testar no Windows:
 *   1. Win+R → digitar joy.cpl → Enter.
 *   2. Selecionar "G27 Pedals USB" → Propriedades.
 *   3. Mover cada pedal: o cursor deve se mover nos eixos X, Y ou Z.
 */

#include <Joystick.h>

// ─── Configuração dos pinos ───────────────────────────────────────────────────
const int PIN_ACELERADOR = A0;
const int PIN_FREIO      = A1;
const int PIN_EMBREAGEM  = A2;

// ─── Inversão de eixo ────────────────────────────────────────────────────────
// Altere para true se o eixo estiver invertido no simulador.
// (potenciômetros com polaridade invertida)
const bool INVERTER_ACELERADOR = false;
const bool INVERTER_FREIO      = false;
const bool INVERTER_EMBREAGEM  = false;

// ─── Faixa de saída do joystick ──────────────────────────────────────────────
// A biblioteca Joystick aceita qualquer faixa inteira.
// 0–1023 corresponde diretamente à resolução ADC de 10 bits.
const int JOYSTICK_MIN = 0;
const int JOYSTICK_MAX = 1023;

// ─── Filtro de média móvel ────────────────────────────────────────────────────
// Suaviza leituras ruidosas dos potenciômetros.
// Aumente AMOSTRAS para mais suavidade (mas adiciona latência).
const int AMOSTRAS = 4;

// Buffers circulares para média móvel de cada eixo
int bufAcel[AMOSTRAS]  = {0};
int bufFreio[AMOSTRAS] = {0};
int bufEmb[AMOSTRAS]   = {0};
int idxBuf = 0;  // índice atual no buffer circular

// ─── Debug Serial ─────────────────────────────────────────────────────────────
// Habilite true para ver valores no Monitor Serial (útil na primeira
// configuração). Desabilite em uso normal para reduzir overhead.
const bool DEBUG_SERIAL = true;
const unsigned long DEBUG_INTERVALO_MS = 100;
unsigned long ultimoDebug = 0;

// ─── Instância do joystick ────────────────────────────────────────────────────
// Parâmetros: ID HID, tipo JOYSTICK_TYPE_JOYSTICK, botões, hat switches,
// habilitar X, Y, Z, Rx, Ry, Rz, rudder, throttle, acelerador, freio, steering
Joystick_ joystick(
    JOYSTICK_DEFAULT_REPORT_ID,  // ID padrão HID
    JOYSTICK_TYPE_JOYSTICK,      // tipo: joystick genérico
    0,                           // 0 botões
    0,                           // 0 hat switches
    true,                        // eixo X  → Acelerador
    true,                        // eixo Y  → Freio
    true,                        // eixo Z  → Embreagem
    false,                       // sem Rx
    false,                       // sem Ry
    false,                       // sem Rz
    false,                       // sem rudder
    false,                       // sem throttle
    false,                       // sem acelerador dedicado
    false,                       // sem freio dedicado
    false                        // sem steering dedicado
);

// ─── setup ────────────────────────────────────────────────────────────────────
void setup() {
    if (DEBUG_SERIAL) {
        Serial.begin(115200);
        // Aguarda conexão USB (necessário no ATmega32U4)
        unsigned long inicio = millis();
        while (!Serial && millis() - inicio < 3000) {
            ; // timeout de 3s para não travar sem Monitor Serial aberto
        }
        Serial.println(F("G27 Pedals USB - iniciando..."));
    }

    // Define a faixa de cada eixo para que a biblioteca normalize corretamente
    joystick.setXAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);
    joystick.setYAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);
    joystick.setZAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);

    // Inicia o dispositivo HID; true = envia relatório automaticamente
    joystick.begin(false);  // false = controle manual do envio (mais eficiente)

    // Pré-popula os buffers com a leitura inicial para evitar spike no boot
    for (int i = 0; i < AMOSTRAS; i++) {
        bufAcel[i]  = analogRead(PIN_ACELERADOR);
        bufFreio[i] = analogRead(PIN_FREIO);
        bufEmb[i]   = analogRead(PIN_EMBREAGEM);
    }
}

// ─── loop ─────────────────────────────────────────────────────────────────────
void loop() {
    // 1. Lê os potenciômetros
    int rawAcel  = analogRead(PIN_ACELERADOR);
    int rawFreio = analogRead(PIN_FREIO);
    int rawEmb   = analogRead(PIN_EMBREAGEM);

    // 2. Insere no buffer circular (média móvel)
    bufAcel[idxBuf]  = rawAcel;
    bufFreio[idxBuf] = rawFreio;
    bufEmb[idxBuf]   = rawEmb;
    idxBuf = (idxBuf + 1) % AMOSTRAS;

    // 3. Calcula a média de cada buffer
    long somaAcel = 0, somaFreio = 0, somaEmb = 0;
    for (int i = 0; i < AMOSTRAS; i++) {
        somaAcel  += bufAcel[i];
        somaFreio += bufFreio[i];
        somaEmb   += bufEmb[i];
    }
    int filtAcel  = somaAcel  / AMOSTRAS;
    int filtFreio = somaFreio / AMOSTRAS;
    int filtEmb   = somaEmb   / AMOSTRAS;

    // 4. Aplica inversão de eixo se configurado
    int valX = INVERTER_ACELERADOR ? (JOYSTICK_MAX - filtAcel)  : filtAcel;
    int valY = INVERTER_FREIO      ? (JOYSTICK_MAX - filtFreio) : filtFreio;
    int valZ = INVERTER_EMBREAGEM  ? (JOYSTICK_MAX - filtEmb)   : filtEmb;

    // 5. Atualiza os eixos e envia o relatório HID
    joystick.setXAxis(valX);
    joystick.setYAxis(valY);
    joystick.setZAxis(valZ);
    joystick.sendState();  // envia somente quando chamamos explicitamente

    // 6. Saída de debug no Monitor Serial (opcional)
    if (DEBUG_SERIAL) {
        unsigned long agora = millis();
        if (agora - ultimoDebug >= DEBUG_INTERVALO_MS) {
            ultimoDebug = agora;
            Serial.print(F("Acel="));
            Serial.print(valX);
            Serial.print(F("  Freio="));
            Serial.print(valY);
            Serial.print(F("  Emb="));
            Serial.println(valZ);
        }
    }

    // Pequena pausa para não saturar o barramento USB
    delay(5);
}
