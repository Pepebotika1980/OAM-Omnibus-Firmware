# OAM Time Machine Firmware - OMNIBUS EDITION v1.0

![OAM Time Machine](https://i.imgur.com/example.jpg) *[Inserte foto del panel aquí]*

## Introducción
Este es el firmware definitivo para el módulo OAM Time Machine. Consolida **5 motores de sonido diferentes** en un solo archivo. Ya no necesitas reprogramar el módulo para cambiar de efecto; simplemente selecciona el modo deseado al encenderlo.

## Cómo Seleccionar un Modo (Boot-Up Selection)
El módulo decide qué motor cargar basándose en la posición del **Slider "1/8t"** (el segundo slider empezando por la izquierda) en el momento de encender la unidad.

**Pasos:**
1. Apaga el módulo (o mantén pulsado RESET).
2. Mueve el slider **"1/8t"** a la zona deseada (ver tabla abajo).
3. Enciende el módulo.
4. Observa el LED parpadear para confirmar el modo.
5. Una vez arrancado, el slider vuelve a su función normal.

| Posición Slider "1/8t" | Modo | Parpadeos LED | Descripción |
| :--- | :--- | :--- | :--- |
| **0% - 20%** (Abajo) | **Studio Reverb** | 1 Blink | Reverb limpia de alta fidelidad |
| **20% - 40%** | **Shimmer Reverb** | 2 Blinks | Reverb con Pitch Shifter angelical |
| **40% - 60%** (Centro) | **SuperMassive** | 3 Blinks | Delay/Reverb experimental y masivo |
| **60% - 80%** | **Resonator** | 4 Blinks | Sintetizador de modelado físico |
| **80% - 100%** (Arriba) | **LEGACY (Original)** | 5 Blinks | El firmware original del Time Machine |

---

## Guía Detallada de Modos

### 1. Studio Reverb (1 Blink)
Una reverb estéreo basada en FDN (Feedback Delay Network) diseñada para sonidos limpios y espaciosos.

*   **Knob "t" (Central):** Tamaño de la sala (Reverb Time base).
    *   *CV Input (t/2^v):* Modula el tamaño.
*   **Knob "Spread" (Izquierda):** Difusión estéreo. Ajusta cuán ancho se siente el espacio.
    *   *CV Input (CV):* Modula la difusión.
*   **Knob "Feedback" (Derecha):** Decay (Tiempo de caída). Aumenta la longitud de la cola de la reverb. 
    *   *CV Input (CV):* Modula el decay.
*   **Slider "Dry":** Volumen de la señal limpia original.
*   **Sliders "1/8t" a "t":** **Ecualizador Espectral**. Cada slider controla el volumen de una banda de frecuencia interna (o delay line específica) de la reverb. Úsalos para "esculpir" el tono de la cola (ej. bajar los graves, subir los agudos).

### 2. Shimmer Reverb (2 Blinks)
Similar a la Studio Reverb, pero añade un cambio de tono (octava arriba) en el bucle de realimentación, creando un sonido de "coro celestial" o "pad" infinito.

*   **Knob "t" (Central):** Tamaño de la sala.
    *   *CV Input (t/2^v):* Modula el tamaño.
*   **Knob "Spread" (Izquierda):** Difusión y carácter del Pitch.
    *   *CV Input (CV):* Modula la difusión.
*   **Knob "Feedback" (Derecha):** Decay etéreo.
    *   *CV Input (CV):* Modula el decay.
*   **Slider "Dry":** Volumen de la señal limpia.
*   **Sliders "1/8t" a "t":** Ecualizador Espectral de la cola Shimmer.

### 3. SuperMassive (3 Blinks)
Un motor experimental inspirado en delays de cinta desgastados y agujeros negros. Combina modulación aleatoria intensa con una arquitectura de delay masiva.

*   **Knob "t" (Central):** Base de Tiempo del Delay (Time Base).
    *   *CV Input (t/2^v):* Control de tiempo.
*   **Knob "Spread" (Izquierda):** **WARP & PITCH**. Controla la cantidad de deriva de tono (wow/flutter) y degradación de la señal.
    *   *CV Input (CV):* Modula el efecto Warp.
*   **Knob "Feedback" (Derecha):** Feedback y **FREEZE**. Al máximo, congela el sonido infinitamente.
    *   *CV Input (CV):* Control de feedback.
*   **Slider "Dry":** Volumen de la señal limpia.
*   **Sliders "1/8t" a "t":** Ganancia de los "Granos" o Taps del delay difuso. Experimenta para cambiar la textura de la nube de sonido.

### 4. Resonator (4 Blinks)
¡Convierte tu módulo en una voz de sintetizador! Usa filtros resonantes afinados para simular cuerdas o campanas.
*Requiere una señal de entrada (audio) para "excitar" el resonador (como golpear una cuerda). Prueba con pulsos, ruido o baterías.*

*   **Knob "t" (Central):** **PITCH (Afinación)**. Selecciona la nota fundamental.
    *   *CV Input (t/2^v):* **Entrada de 1V/Oct** (No calibrada perfectamente, pero funcional para melodías).
*   **Knob "Spread" (Izquierda):** **STRUCTURE (Estructura)**. Cambia la relación de los armónicos.
    *   Min: Cuerda armónica.
    *   Centro: Sonidos inarmónicos / metálicos.
    *   Max: Campana / Plato.
    *   *CV Input (CV):* Modula la estructura/timbre.
*   **Knob "Feedback" (Derecha):** **DAMPING (Amortiguación)**. Controla cuánto tiempo resuena la nota. 
    *   mínimo: Sonido seco y corto.
    *   máximo: Sustain largo como un pad.
    *   *CV Input (CV):* Modula la duración de la nota.
*   **Slider "Dry":** Mezcla la señal excitadora original (el golpe).
*   **Sliders "1/8t" a "t":** **MIXER DE ARMÓNICOS**. 
    *   "1/8t" es la fundamental (Nota base).
    *   Los siguientes sliders son armónicos superiores. Úsalos como un órgano de barras para crear el timbre.

### 5. LEGACY MODE (5 Blinks)
Este modo carga **exactamente** el firmware original del Time Machine. Todas las funciones son idénticas al manual original.

*   **Knob "t" (Central):** Master Time (Tiempo de delay).
*   **Knob "Spread" (Izquierda):** Spacing/Skew de los cabezales.
*   **Knob "Feedback" (Derecha):** Feedback general.
*   **Slider "Dry":** Nivel Dry.
*   **Sliders "1/8t" a "t":** Nivel de volumen para cada tap de delay individual (1/8 del tiempo, 1/4, etc.).
*   **Entradas CV:** Funcionan exactamente igual que en el diseño original.

---
**Desarrollado por OAM Firmware Division.**
*Disfruta de tu viaje en el tiempo.*
