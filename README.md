# G27 Pedals USB — Arduino Pro Micro (ATmega32U4)

Transforma os pedais do Logitech G27 em um joystick USB independente usando
um Arduino Pro Micro 5V/16MHz.

---

## Estrutura do projeto

```
g27/
├── diagnostico/
│   └── diagnostico.ino   ← PASSO 1: confirmar que os potenciômetros funcionam
└── g27_pedals_hid/
    └── g27_pedals_hid.ino ← PASSO 2: firmware HID joystick completo
```

---

## Ligações físicas

| Fio do conector dos pedais | Arduino Pro Micro |
|---------------------------|-------------------|
| 5V (alimentação)          | VCC               |
| GND                       | GND               |
| Sinal Acelerador          | A0                |
| Sinal Freio               | A1                |
| Sinal Embreagem           | A2                |

---

## Passo 1 — Diagnóstico (faça isso primeiro)

1. Abra `diagnostico/diagnostico.ino` na Arduino IDE.
2. Selecione a placa: **Arduino Leonardo** (mesmo chip ATmega32U4).
3. Selecione a porta COM correta.
4. Clique em **Upload**.
5. Abra **Ferramentas → Monitor Serial** a **115200 baud**.
6. Pressione e solte cada pedal lentamente.

**Resultado esperado:**

```
A0   | Acelerador  |  512 |   0  | 1023  [##########..........]
A1   | Freio       |  128 |   0  | 1023  [###.................]
A2   | Embreagem   |  895 |   0  | 1023  [#################...]
```

- Cada pedal deve variar entre ~0 e ~1023.
- Se um valor ficar fixo em 0 ou 1023 o tempo todo → verifique a ligação.
- Se variar suavemente → potenciômetro **OK**, prossiga para o Passo 2.

---

## Passo 2 — Firmware HID Joystick

### Instalar a biblioteca

1. Arduino IDE → **Sketch → Incluir Biblioteca → Gerenciar Bibliotecas**.
2. Pesquisar: `Joystick` por **Matthew Heironimus**.
3. Clicar em **Instalar**.

### Gravar o firmware

1. Abra `g27_pedals_hid/g27_pedals_hid.ino`.
2. Placa: **Arduino Leonardo**.
3. Porta: porta COM do Pro Micro.
4. **Upload**.

### Testar no Windows

1. **Win+R** → digitar `joy.cpl` → Enter.
2. Selecionar **"G27 Pedals USB"** → **Propriedades**.
3. Mover cada pedal: o marcador deve se mover nos eixos X, Y e Z.

---

## Configurações ajustáveis (g27_pedals_hid.ino)

| Constante             | Padrão  | Descrição                                      |
|-----------------------|---------|------------------------------------------------|
| `INVERTER_ACELERADOR` | `false` | `true` se o eixo estiver invertido             |
| `INVERTER_FREIO`      | `false` | `true` se o eixo estiver invertido             |
| `INVERTER_EMBREAGEM`  | `false` | `true` se o eixo estiver invertido             |
| `AMOSTRAS`            | `4`     | Tamanho do filtro (mais = mais suave, + lento) |
| `DEBUG_SERIAL`        | `true`  | `false` desativa saída serial em uso normal    |

---

## Compatibilidade

- Testado com Arduino IDE 1.8.x e 2.x
- Biblioteca Joystick v2.x (Matthew Heironimus)
- Windows 10/11 — `joy.cpl`
- Simuladores: Assetto Corsa, iRacing, rFactor, BeamNG, etc.

## Notas de versão

- v1.0 — firmware inicial
