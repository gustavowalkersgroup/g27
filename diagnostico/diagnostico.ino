/*
 * G27 Pedals - Sketch de Diagnóstico
 * Placa: Arduino Pro Micro (ATmega32U4, 5V, 16MHz)
 *
 * Objetivo: Confirmar que os três potenciômetros dos pedais
 * estão funcionando ANTES de instalar o firmware HID.
 *
 * Ligações:
 *   Fio de 5V  do conector dos pedais  → VCC do Arduino
 *   Fio GND    do conector dos pedais  → GND do Arduino
 *   Sinal Acelerador                   → A0
 *   Sinal Freio                        → A1
 *   Sinal Embreagem                    → A2
 *
 * Como usar:
 *   1. Grave este sketch no Pro Micro.
 *   2. Abra o Monitor Serial (Ferramentas → Monitor Serial).
 *   3. Ajuste a velocidade para 115200 baud.
 *   4. Pressione cada pedal completamente e solte.
 *   5. Verifique que cada eixo varia entre ~0 e ~1023.
 *      - Se o valor ficar fixo ou em 0/1023 o tempo todo,
 *        o potenciômetro pode estar danificado.
 *      - Variação suave de 0→1023 = potenciômetro OK.
 */

// Pinos analógicos dos pedais
const int PIN_ACELERADOR  = A0;
const int PIN_FREIO       = A1;
const int PIN_EMBREAGEM   = A2;

// Intervalo entre leituras (ms)
const unsigned long INTERVALO_MS = 50;

// Acumuladores de mínimo e máximo para cada eixo
int minAcel = 1023, maxAcel = 0;
int minFreio = 1023, maxFreio = 0;
int minEmb = 1023, maxEmb = 0;

unsigned long ultimaLeitura = 0;

void setup() {
    // Inicia Serial; Pro Micro usa USB CDC nativo,
    // então Serial = SerialUSB automaticamente.
    Serial.begin(115200);

    // Aguarda o host USB conectar (necessário no ATmega32U4)
    while (!Serial) {
        ; // espera até o Monitor Serial abrir
    }

    Serial.println(F("=============================================="));
    Serial.println(F("  G27 Pedals - Diagnostico de Potenciometros "));
    Serial.println(F("=============================================="));
    Serial.println(F("Pino | Pedal       | Bruto | Min  | Max"));
    Serial.println(F("-----|-------------|-------|------|-----"));
}

void loop() {
    unsigned long agora = millis();
    if (agora - ultimaLeitura < INTERVALO_MS) return;
    ultimaLeitura = agora;

    // Leitura dos três canais analógicos (resolução 10 bits: 0–1023)
    int valAcel  = analogRead(PIN_ACELERADOR);
    int valFreio = analogRead(PIN_FREIO);
    int valEmb   = analogRead(PIN_EMBREAGEM);

    // Atualiza mínimos e máximos observados na sessão
    minAcel  = min(minAcel,  valAcel);
    maxAcel  = max(maxAcel,  valAcel);
    minFreio = min(minFreio, valFreio);
    maxFreio = max(maxFreio, valFreio);
    minEmb   = min(minEmb,   valEmb);
    maxEmb   = max(maxEmb,   valEmb);

    // Exibe tabela de fácil leitura no Monitor Serial
    imprimirLinha("A0", "Acelerador ", valAcel,  minAcel,  maxAcel);
    imprimirLinha("A1", "Freio      ", valFreio, minFreio, maxFreio);
    imprimirLinha("A2", "Embreagem  ", valEmb,   minEmb,   maxEmb);

    Serial.println(F("-------------------------------------------"));
}

/*
 * Formata e imprime uma linha da tabela de diagnóstico.
 * Parâmetros:
 *   pino  - nome do pino (ex: "A0")
 *   nome  - nome do pedal
 *   bruto - leitura ADC atual (0–1023)
 *   vmin  - menor valor visto desde o boot
 *   vmax  - maior valor visto desde o boot
 */
void imprimirLinha(const char* pino, const char* nome,
                   int bruto, int vmin, int vmax) {
    // Constrói a barra de progresso visual (20 chars = 1023 unidades)
    int barras = map(bruto, 0, 1023, 0, 20);
    char barra[22];
    for (int i = 0; i < 20; i++) {
        barra[i] = (i < barras) ? '#' : '.';
    }
    barra[20] = '\0';

    // Imprime linha formatada
    Serial.print(pino);
    Serial.print(F("   | "));
    Serial.print(nome);
    Serial.print(F(" | "));
    // Pad do valor bruto para 4 dígitos
    if (bruto < 1000) Serial.print(' ');
    if (bruto < 100)  Serial.print(' ');
    if (bruto < 10)   Serial.print(' ');
    Serial.print(bruto);
    Serial.print(F(" | "));
    if (vmin < 100) Serial.print(' ');
    if (vmin < 10)  Serial.print(' ');
    Serial.print(vmin);
    Serial.print(F(" | "));
    if (vmax < 100) Serial.print(' ');
    if (vmax < 10)  Serial.print(' ');
    Serial.print(vmax);
    Serial.print(F("  ["));
    Serial.print(barra);
    Serial.println(F("]"));
}
