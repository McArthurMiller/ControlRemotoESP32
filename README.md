# Control Remoto ESP32 — Teclado + Display LED TARIF

Sistema emisor-receptor con dos ESP32. El **emisor** tiene un teclado 4x4, una pantalla LCD 16x2 y un sensor de temperatura y humedad DHT22; el operador captura números de hasta 3 dígitos y los envía por serial. El **receptor** valida cada trama y muestra el número en una tarjeta de display LED de 7 segmentos (TARIF).

La comunicación usa un **protocolo serial propio** con delimitadores STX/ETX y bytes de comando.

---

## Arquitectura

```
┌──────────────────────────────┐                    ┌──────────────────────────────┐
│        EMISOR (ESP32)        │                    │       RECEPTOR (ESP32)       │
│                              │   UART 9600 baud   │                              │
│  Teclado 4x4 ──┐             │  TX ───────────▶ RX│  Máquina de estados          │
│                ├─▶ Lógica ───┼────────────────────┼─▶ (valida STX … ETX)         │
│  DHT22 ────────┘     │       │  GND ─────── GND   │          │                   │
│                      ▼       │                    │          ▼                   │
│              LCD 16x2        │                    │  Display TARIF (4 dígitos)   │
│  fila 0: temp / humedad      │                    │  registros de desplazamiento │
│  fila 1: mensajes teclado    │                    │  + brillo por PWM            │
└──────────────────────────────┘                    └──────────────────────────────┘
```

---

## Protocolo serial

Cada trama mide 6 bytes:

| Byte | Contenido | Valor |
|---|---|---|
| 0 | STX (inicio) | `0x02` |
| 1–3 | Número en ASCII, 3 dígitos con ceros a la izquierda | `'0'`–`'9'`, o `0x20` (espacio) si no hay número |
| 4 | Comando | ver tabla |
| 5 | ETX (fin) | `0x03` |

**Ejemplo:** enviar el número 7 → `02 30 30 37 2A 03` (`STX "007" '*' ETX`)

### Comandos

| Comando | Byte | Tecla en el emisor |
|---|---|---|
| Enviar número | `0x2A` (`*`) | `A` |
| Borrar número | `0x55` (`U`) | `B` |
| Borrar todos | `0x43` (`C`) | `C` + confirmación con `1` |
| Prueba | `0x23` (`#`) | `#` |

---

## Emisor — `Emisor_Teclado/`

### Teclado

| Tecla | Acción |
|---|---|
| `0`–`9` | Captura dígitos en un buffer deslizante (FIFO) de 3 posiciones |
| `A` | Envía el número capturado; si el buffer está vacío, reenvía el último número |
| `B` | Borra el número capturado (o el último) de la lista y envía el comando de borrado |
| `C` | Pide confirmación en el LCD: `1` borra todo, `2` cancela |
| `*` | Limpia la fila de mensajes del LCD y el buffer |
| `#` | Envía la trama de prueba |
| `D` | Sale del modo de prueba |

Además, el emisor mantiene una lista de hasta 10 números activos: los agrega al enviar y los quita al borrar.

### Detalles técnicos

- **Escaneo por multiplexado:** activa una fila a la vez (`LOW`) y lee las 4 columnas con pull-up interno.
- **Antirrebote no bloqueante:** escanea cada 50 ms con `millis()` y solo procesa una tecla cuando cambia respecto a la lectura anterior.
- **LCD con zonas separadas:** la fila 0 se reserva para el sensor y la fila 1 para los mensajes del teclado.
- **Lectura del sensor cada 10 s**, solo cuando el sistema está inactivo (buffer vacío y sin confirmación pendiente), para no sobrescribir lo que el operador está capturando.
- **Manejo de errores del sensor** con `isnan()`.

### Pines

| Función | GPIO |
|---|---|
| LCD RS, E | 4, 5 |
| LCD D4–D7 | 18, 19, 21, 22 |
| Teclado filas | 26, 25, 33, 32 |
| Teclado columnas | 13, 12, 14, 27 |
| DHT22 (datos) | 17 |

---

## Receptor — `Receptor_TARIF/`

### Detalles técnicos

- **Máquina de estados de 6 posiciones** (`posicion` 0–5) que procesa la trama byte por byte. Ignora todo hasta recibir STX y descarta la trama si el último byte no es ETX.
- **Codificación a 7 segmentos** con una tabla `switch` y envío con `shiftOut()` a los registros de desplazamiento de la tarjeta.
- **Brillo por PWM** sobre el pin de habilitación de salida (OE): 5 kHz, resolución de 8 bits.
- En esta versión, el receptor muestra el número de cada trama válida. El byte de comando se guarda en `comandoRecibido` para acciones futuras.

### Pines

| Función | GPIO |
|---|---|
| DATA | 15 |
| CLOCK | 26 |
| LATCH | 2 |
| OE (PWM de brillo) | 22 |

---

## Requisitos y compilación

**Arduino IDE** (probado con 1.8.19) con soporte para ESP32.

| Sketch | Librerías | Versión del core ESP32 |
|---|---|---|
| Emisor | `LiquidCrystal`, **DHT sensor library** (Adafruit) + Adafruit Unified Sensor | Compila con 3.x |
| Receptor | — | **Requiere 2.x** |

> **Nota sobre el receptor:** usa `ledcSetup()` y `ledcAttachPin()`, que se eliminaron en el core 3.x del ESP32 (reemplazadas por `ledcAttach()`). Se publica la versión probada en hardware. Para compilarlo, instala el core 2.0.x desde *Herramientas → Placa → Gestor de tarjetas*.

---

## Evolución del proyecto

**Emisor**
- **V1:** teclado 4x4, LCD 16x2 y protocolo serial STX/ETX.
- **V2:** sensor DS18B20 (solo temperatura) y lista de números enviados.
- **V3:** cambio a DHT22 (temperatura y humedad) en dos zonas independientes del LCD.
- **V4:** pruebas con un segundo sensor para validar lecturas; versión final con un DHT22.

**Receptor** (27/08/26 – 04/09/26)
- **V01–V03:** envío bit a bit al display y tabla de dígitos.
- **V04:** control de brillo por PWM.
- **V05:** rutina de prueba de dígitos.
- **V06–V07:** rutina de recepción y validación de la trama.
- **V08:** prueba de integración con el emisor: la tarjeta responde a las órdenes del teclado.

---

## Posibles mejoras

- Migrar el PWM del receptor a la API del core 3.x (`ledcAttach`).
- Usar el byte de comando en el receptor (borrar, prueba) en lugar de solo mostrar el número.
- Mostrar correctamente el `0` y rellenar con espacios los números de menos de 4 dígitos en `mostrarByte()`.
- Separar los mensajes de depuración del canal serial que usa el protocolo.
- Agregar un byte de verificación (checksum) a la trama.

---

## Autor

**Arturo Jiménez Hernández** — [@McArthurMiller](https://github.com/McArthurMiller)
