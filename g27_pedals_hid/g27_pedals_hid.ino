/*
 * G27 Pedals + Freio de Mão + Botões - Firmware HID Joystick
 * Placa: Arduino Pro Micro (ATmega32U4, 5V, 16MHz)
 *
 * Dependência obrigatória (gratuita):
 *   Biblioteca "Joystick" de Matthew Heironimus
 *   Sketch → Incluir Biblioteca → Gerenciar Bibliotecas → "Joystick"
 *
 * ── EIXOS ANALÓGICOS (todos LINEARES, sem X/Y) ────────────────────
 *   No joy.cpl, X+Y viram um gráfico 2D com cruz. Para cada pedal
 *   aparecer como uma BARRA independente, usamos só Z/Rx/Ry/Rz.
 *
 *   A0  →  Eixo Z      (Acelerador)
 *   A1  →  Rotação X   (Freio)
 *   A2  →  Rotação Y   (Embreagem)
 *   A3  →  Rotação Z   (Freio de Mão)
 *
 * ── CALIBRAÇÃO ────────────────────────────────────────────────────
 *   MODO MANUAL (padrão): cada eixo usa uma faixa FIXA definida em
 *   CAL_MIN/CAL_MAX abaixo. Previsível: um eixo nunca influencia o
 *   outro. Para descobrir os valores reais dos seus potenciômetros,
 *   ligue DEBUG_SERIAL e leia os valores "brutos" no Monitor Serial
 *   com o pedal solto e pisado, depois ajuste as constantes.
 *
 *   MODO AUTOMÁTICO (CALIBRACAO_AUTO = true): aprende min/max em
 *   tempo real. Evite se houver ruído/vazamento entre canais — o
 *   estiramento da faixa amplifica qualquer interferência.
 *
 *   Observação sobre 5V vs 3,3V: a leitura é RATIOMÉTRICA — o
 *   potenciômetro e o ADC usam a mesma referência (VCC), então a
 *   tensão da placa não distorce os valores. O máximo ficar em ~950
 *   em vez de 1023 é só porque o braço do pedal não gira o
 *   potenciômetro até o fim — exatamente o que a calibração corrige.
 *
 * ── BOTÕES DIGITAIS ───────────────────────────────────────────────
 *   Chave mecânica: um terminal no pino, outro terminal no GND.
 *   Sem resistor externo (usa pull-up interno do Arduino).
 *
 *   D2   →  Botão  1       D9   →  Botão  8
 *   D3   →  Botão  2       D10  →  Botão  9
 *   D4   →  Botão  3       D14  →  Botão 10
 *   D5   →  Botão  4       D15  →  Botão 11
 *   D6   →  Botão  5       D16  →  Botão 12
 *   D7   →  Botão  6
 *   D8   →  Botão  7
 *                                    Total: 12 botões
 *
 * ── COMPILAÇÃO ────────────────────────────────────────────────────
 *   Ferramentas → Placa  → Arduino Leonardo
 *   Ferramentas → Porta  → COMX (porta do Pro Micro)
 *   Upload (→)
 *
 * ── TESTE ─────────────────────────────────────────────────────────
 *   Win+R → joy.cpl → Propriedades
 *   No modo manual os pedais respondem imediatamente; no modo
 *   automático, pise fundo em cada pedal uma vez (calibração).
 */

#include <Joystick.h>

// ── Pinos analógicos ──────────────────────────────────────────────
const int PIN_ACELERADOR  = A0;
const int PIN_FREIO       = A1;
const int PIN_EMBREAGEM   = A2;
const int PIN_FREIO_MAO   = A3;

// ── Inversão de eixo ─────────────────────────────────────────────
// Nos pedais G27 o potenciômetro lê ALTO em repouso e BAIXO
// pressionado, por isso os três começam invertidos (true).
// O freio de mão depende de como você ligar o potenciômetro;
// se responder ao contrário, mude para true.
const bool INVERTER_ACELERADOR = true;
const bool INVERTER_FREIO      = true;
const bool INVERTER_EMBREAGEM  = true;
const bool INVERTER_FREIO_MAO  = false;

// ── Pinos dos botões ─────────────────────────────────────────────
const int NUM_BOTOES = 12;
const int PINOS_BOTOES[NUM_BOTOES] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 14, 15, 16};

// ── Faixa de saída dos eixos ──────────────────────────────────────
const int JOYSTICK_MIN = 0;
const int JOYSTICK_MAX = 1023;

// ── Calibração ───────────────────────────────────────────────────
// false = MANUAL (recomendado): usa as faixas fixas CAL_MIN/CAL_MAX.
// true  = AUTOMÁTICA: aprende min/max em tempo real (sensível a
//         ruído/vazamento — pode "misturar" os eixos).
const bool CALIBRACAO_AUTO = false;

// Faixas fixas do modo manual (valores BRUTOS do ADC, 0–1023),
// medidas no Monitor Serial com cada pedal solto e pisado.
// ATENÇÃO: estes valores estão muito baixos para um pot alimentado
// com 5V (repouso deveria ler ~950). Verifique o fio de VCC dos
// potenciômetros; com a fiação corrigida, meça de novo e atualize.
// Ordem: Acel, Freio, Embreagem, F. Mão.
const int CAL_MIN[4] = { 140,  90,  29,  60 };
const int CAL_MAX[4] = { 390, 155,  42, 960 };

// Parâmetros do modo automático:
// SPAN_MINIMO: curso mínimo (contagens ADC) para considerar o eixo
// calibrado; antes disso ele fica fixo em 0 (repouso).
const int SPAN_MINIMO = 100;
// MARGEM_PCT: porcentagem cortada em cada extremidade da faixa
// aprendida, garantindo que o pedal alcance 0 e 1023 com folga.
const float MARGEM_PCT = 0.03;

// ── Filtro de média móvel ────────────────────────────────────────
// Aumentar AMOSTRAS = mais suave, porém adiciona latência
// 8 amostras enquanto as faixas medidas são estreitas (pouco sinal);
// com a fiação dos pots corrigida, 4 volta a ser suficiente.
const int AMOSTRAS = 8;

// ── Debug Serial ─────────────────────────────────────────────────
// true só para diagnóstico; false em uso normal (mais estável)
const bool DEBUG_SERIAL = false;
const unsigned long DEBUG_INTERVALO_MS = 100;
unsigned long ultimoDebug = 0;

// ── Estrutura de cada eixo: filtro + calibração ──────────────────
struct Eixo {
    int  pino;       // pino analógico
    bool inverter;   // inverte o sentido após calibrar
    int  buf[AMOSTRAS];
    int  minObs;     // menor valor já observado
    int  maxObs;     // maior valor já observado
    int  valor;      // saída final 0–1023
    int  bruto;      // última leitura filtrada (para debug/calibração)
};

Eixo eixos[4] = {
    { PIN_ACELERADOR, INVERTER_ACELERADOR, {0}, 1023, 0, 0 },
    { PIN_FREIO,      INVERTER_FREIO,      {0}, 1023, 0, 0 },
    { PIN_EMBREAGEM,  INVERTER_EMBREAGEM,  {0}, 1023, 0, 0 },
    { PIN_FREIO_MAO,  INVERTER_FREIO_MAO,  {0}, 1023, 0, 0 },
};

int idxBuf = 0;

// ── Instância do joystick ────────────────────────────────────────
// Parâmetros: ID, tipo, botões, hats, X, Y, Z, Rx, Ry, Rz, rudder,
//             throttle, accel, brake, steering
// X e Y DESABILITADOS de propósito: assim nenhum pedal entra no
// gráfico 2D do joy.cpl — todos aparecem como barras lineares.
Joystick_ joystick(
    JOYSTICK_DEFAULT_REPORT_ID,
    JOYSTICK_TYPE_JOYSTICK,
    NUM_BOTOES,   // 12 botões
    0,            // sem hat switch
    false,        // X  desabilitado
    false,        // Y  desabilitado
    true,         // Z  → Acelerador
    true,         // Rx → Freio
    true,         // Ry → Embreagem
    true,         // Rz → Freio de Mão
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
    joystick.setZAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);
    joystick.setRxAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);
    joystick.setRyAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);
    joystick.setRzAxisRange(JOYSTICK_MIN, JOYSTICK_MAX);

    joystick.begin(false); // false = envio manual via sendState()

    // Pré-popula buffers e inicia min/max na leitura atual,
    // assim a calibração parte do ponto de repouso real
    for (int e = 0; e < 4; e++) {
        int leitura = lerAnalogico(eixos[e].pino);
        for (int i = 0; i < AMOSTRAS; i++) {
            eixos[e].buf[i] = leitura;
        }
        eixos[e].minObs = leitura;
        eixos[e].maxObs = leitura;
    }
}

/*
 * Leitura analógica sem "vazamento" entre canais.
 * O ATmega32U4 tem UM ADC compartilhado por A0–A3; ao trocar de
 * canal, o capacitor interno de sample-and-hold ainda guarda carga
 * da leitura anterior, fazendo um pedal influenciar o outro
 * (acelerador "vazando" no freio etc.), mesmo com tudo aterrado.
 * Solução: após selecionar o canal, faz uma leitura descartável,
 * espera o capacitor assentar e só então faz a leitura válida.
 */
int lerAnalogico(int pino) {
    analogRead(pino);          // descartada: só troca o canal e carrega o S/H
    delayMicroseconds(100);    // tempo de assentamento
    return analogRead(pino);   // leitura válida
}

/*
 * Processa um eixo: filtra, atualiza calibração e converte a
 * leitura bruta para a faixa completa 0–1023.
 */
void processarEixo(Eixo &e, int idx) {
    // 1. Média móvel
    e.buf[idxBuf] = lerAnalogico(e.pino);
    long soma = 0;
    for (int i = 0; i < AMOSTRAS; i++) soma += e.buf[i];
    int filtrado = soma / AMOSTRAS;
    e.bruto = filtrado;

    int lo, hi;

    if (CALIBRACAO_AUTO) {
        // 2a. Atualiza limites aprendidos
        if (filtrado < e.minObs) e.minObs = filtrado;
        if (filtrado > e.maxObs) e.maxObs = filtrado;

        int span = e.maxObs - e.minObs;

        // Eixo ainda não movido o suficiente → mantém em repouso (0)
        if (span < SPAN_MINIMO) {
            e.valor = 0;
            return;
        }

        // Corta margem nas extremidades para garantir fundo de escala
        int margem = (int)(span * MARGEM_PCT);
        lo = e.minObs + margem;
        hi = e.maxObs - margem;
    } else {
        // 2b. Faixa fixa: cada eixo só depende da própria constante
        lo = CAL_MIN[idx];
        hi = CAL_MAX[idx];
    }

    int v = constrain(filtrado, lo, hi);
    v = map(v, lo, hi, JOYSTICK_MIN, JOYSTICK_MAX);

    // 3. Inversão de sentido (repouso = 0, fundo = 1023)
    if (e.inverter) v = JOYSTICK_MAX - v;

    e.valor = v;
}

void loop() {
    // ── 1. Processa os 4 eixos (filtro + calibração + inversão) ──
    for (int e = 0; e < 4; e++) {
        processarEixo(eixos[e], e);
    }
    idxBuf = (idxBuf + 1) % AMOSTRAS;

    joystick.setZAxis(eixos[0].valor);   // Acelerador
    joystick.setRxAxis(eixos[1].valor);  // Freio
    joystick.setRyAxis(eixos[2].valor);  // Embreagem
    joystick.setRzAxis(eixos[3].valor);  // Freio de Mão

    // ── 2. Leitura dos botões ─────────────────────────────────────
    for (int i = 0; i < NUM_BOTOES; i++) {
        // LOW = chave fechada (pressionada) por causa do pull-up
        bool pressionado = (digitalRead(PINOS_BOTOES[i]) == LOW);
        if (pressionado != estadoAnterior[i]) {
            joystick.setButton(i, pressionado);
            estadoAnterior[i] = pressionado;
        }
    }

    // ── 3. Envia relatório HID ────────────────────────────────────
    joystick.sendState();

    // ── 4. Debug Serial ───────────────────────────────────────────
    if (DEBUG_SERIAL) {
        unsigned long agora = millis();
        if (agora - ultimoDebug >= DEBUG_INTERVALO_MS) {
            ultimoDebug = agora;
            Serial.print(F("Acel="));  Serial.print(eixos[0].valor);
            Serial.print(F("(")); Serial.print(eixos[0].bruto); Serial.print(F(")"));
            Serial.print(F(" Freio=")); Serial.print(eixos[1].valor);
            Serial.print(F("(")); Serial.print(eixos[1].bruto); Serial.print(F(")"));
            Serial.print(F(" Emb="));  Serial.print(eixos[2].valor);
            Serial.print(F("(")); Serial.print(eixos[2].bruto); Serial.print(F(")"));
            Serial.print(F(" FMao=")); Serial.print(eixos[3].valor);
            Serial.print(F("(")); Serial.print(eixos[3].bruto); Serial.print(F(")"));
            Serial.print(F(" Btn="));
            for (int i = 0; i < NUM_BOTOES; i++) {
                Serial.print(estadoAnterior[i] ? "1" : "0");
            }
            Serial.println();
        }
    }

    delay(5);
}
