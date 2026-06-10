/*
 * G27 Pedals + Freio de Mão + Botões - Firmware HID Joystick
 * Placa: Arduino Pro Micro (ATmega32U4, 5V, 16MHz)
 *
 * Dependência obrigatória (gratuita):
 *   Biblioteca "Joystick" de Matthew Heironimus
 *   Sketch → Incluir Biblioteca → Gerenciar Bibliotecas → "Joystick"
 *
 * ── EIXOS ANALÓGICOS ──────────────────────────────────────────────
 *   A0  →  X Axis   (Acelerador)
 *   A1  →  Y Axis   (Freio)
 *   A2  →  Z Axis   (Embreagem)
 *   A3  →  Rx Axis  (Freio de Mão)
 *
 * ── BOTÕES DIGITAIS ───────────────────────────────────────────────
 *   Chave mecânica: um terminal no pino, outro terminal no GND.
 *   Sem resistor externo (usa pull-up interno do Arduino).
 *
 *   D2   →  Botão  0       D9   →  Botão  7
 *   D3   →  Botão  1       D10  →  Botão  8
 *   D4   →  Botão  2       D14  →  Botão  9
 *   D5   →  Botão  3       D15  →  Botão 10
 *   D6   →  Botão  4       D16  →  Botão 11
 *   D7   →  Botão  5
 *   D8   →  Botão  6
 *                                    Total: 12 botões
 *
 * ── COMPILAÇÃO ────────────────────────────────────────────────────
 *   Ferramentas → Placa  → Arduino Leonardo
 *   Ferramentas → Porta  → COMX (porta do Pro Micro)
 *   Upload (→)
 *
 * ── TESTE ─────────────────────────────────────────────────────────
 *   Win+R → joy.cpl → G27 Controller → Propriedades
 */

#include <Joystick.h>

// ── Pinos analógicos ──────────────────────────────────────────────
const int PIN_ACELERADOR  = A0;
const int PIN_FREIO       = A1;
const int PIN_EMBREAGEM   = A2;
const int PIN_FREIO_MAO   = A3;

// ── Inversão de eixo ─────────────────────────────────────────────
// true = inverte o sentido (útil se o pedal responde ao contrário)
const bool INVERTER_ACELERADOR = false;
const bool INVERTER_FREIO      = false;
const bool INVERTER_EMBREAGEM  = false;
const bool INVERTER_FREIO_MAO  = false;

// ── Pinos dos botões ─────────────────────────────────────────────
const int NUM_BOTOES = 12;
const int PINOS_BOTOES[NUM_BOTOES] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 14, 15, 16};

// ── Faixa de saída dos eixos ──────────────────────────────────────
const int JOYSTICK_MIN = 0;
const int JOYSTICK_MAX = 1023;

// ── Filtro de média móvel ────────────────────────────────────────
// Aumentar AMOSTRAS = mais suave, porém adiciona latência
const int AMOSTRAS = 4;
int bufAcel[AMOSTRAS]  = {0};
int bufFreio[AMOSTRAS] = {0};
int bufEmb[AMOSTRAS]   = {0};
int bufFMao[AMOSTRAS]  = {0};
int idxBuf = 0;

// ── Debug Serial ─────────────────────────────────────────────────
// false em uso normal para reduzir overhead
const bool DEBUG_SERIAL = true;
const unsigned long DEBUG_INTERVALO_MS = 100;
unsigned long ultimoDebug = 0;

// ── Instância do joystick ────────────────────────────────────────
// Parâmetros: ID, tipo, botões, hats, X, Y, Z, Rx, Ry, Rz, rudder,
//             throttle, accel, brake, steering
Joystick_ joystick(
    JOYSTICK_DEFAULT_REPORT_ID,
    JOYSTICK_TYPE_JOYSTICK,
    NUM_BOTOES,   // 12 botões
    0,            // sem hat switch
    true,         // X  → Acelerador
    true,         // Y  → Freio
    true,         // Z  → Embreagem
    true,         // Rx → Freio de Mão
    false,
    false,
    false,
    false,
    false,
    false,
    false
);

// ── Debounce dos botões ──────────────────────────────────────────
bool estadoAnterior[NUM_BOTOES] = {false};

void setup() {
    // Configura pinos dos botões com pull-up interno
    // (chave fecha para GND → leitura LOW = pressionado)
    for (int i = 0; i < NUM_BOTOES; i++) {
        pinMode(PINOS_BOTOES[i], INPUT_PULLUP);
    }

    if (DEBUG_SERIAL) {
        Serial.begin(115200);
        unsigned long inicio = millis();
        while (!Serial && millis() - inicio < 3000) { ; }
        Serial.println(F("G27 Controller - iniciando..."));
    }

    // Define faixa de cada eixo
    joystick.setXAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);
    joystick.setYAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);
    joystick.setZAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);
    joystick.setRxAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);

    joystick.begin(false); // false = envio manual via sendState()

    // Pré-popula buffers com leitura inicial para evitar spike
    for (int i = 0; i < AMOSTRAS; i++) {
        bufAcel[i]  = analogRead(PIN_ACELERADOR);
        bufFreio[i] = analogRead(PIN_FREIO);
        bufEmb[i]   = analogRead(PIN_EMBREAGEM);
        bufFMao[i]  = analogRead(PIN_FREIO_MAO);
    }
}

void loop() {
    // ── 1. Leitura analógica ──────────────────────────────────────
    bufAcel[idxBuf]  = analogRead(PIN_ACELERADOR);
    bufFreio[idxBuf] = analogRead(PIN_FREIO);
    bufEmb[idxBuf]   = analogRead(PIN_EMBREAGEM);
    bufFMao[idxBuf]  = analogRead(PIN_FREIO_MAO);
    idxBuf = (idxBuf + 1) % AMOSTRAS;

    long sA = 0, sF = 0, sE = 0, sM = 0;
    for (int i = 0; i < AMOSTRAS; i++) {
        sA += bufAcel[i];
        sF += bufFreio[i];
        sE += bufEmb[i];
        sM += bufFMao[i];
    }
    int valX = sA / AMOSTRAS;
    int valY = sF / AMOSTRAS;
    int valZ = sE / AMOSTRAS;
    int valRx = sM / AMOSTRAS;

    // ── 2. Inversão de eixo ───────────────────────────────────────
    if (INVERTER_ACELERADOR) valX  = JOYSTICK_MAX - valX;
    if (INVERTER_FREIO)      valY  = JOYSTICK_MAX - valY;
    if (INVERTER_EMBREAGEM)  valZ  = JOYSTICK_MAX - valZ;
    if (INVERTER_FREIO_MAO)  valRx = JOYSTICK_MAX - valRx;

    joystick.setXAxis(valX);
    joystick.setYAxis(valY);
    joystick.setZAxis(valZ);
    joystick.setRxAxis(valRx);

    // ── 3. Leitura dos botões ─────────────────────────────────────
    for (int i = 0; i < NUM_BOTOES; i++) {
        // LOW = chave fechada (pressionada) por causa do pull-up
        bool pressionado = (digitalRead(PINOS_BOTOES[i]) == LOW);
        if (pressionado != estadoAnterior[i]) {
            joystick.setButton(i, pressionado);
            estadoAnterior[i] = pressionado;
        }
    }

    // ── 4. Envia relatório HID ────────────────────────────────────
    joystick.sendState();

    // ── 5. Debug Serial ───────────────────────────────────────────
    if (DEBUG_SERIAL) {
        unsigned long agora = millis();
        if (agora - ultimoDebug >= DEBUG_INTERVALO_MS) {
            ultimoDebug = agora;
            Serial.print(F("Acel="));  Serial.print(valX);
            Serial.print(F(" Freio=")); Serial.print(valY);
            Serial.print(F(" Emb="));  Serial.print(valZ);
            Serial.print(F(" FMao=")); Serial.print(valRx);
            // Mostra quais botões estão pressionados
            Serial.print(F(" Btn="));
            for (int i = 0; i < NUM_BOTOES; i++) {
                Serial.print(estadoAnterior[i] ? "1" : "0");
            }
            Serial.println();
        }
    }

    delay(5);
}
