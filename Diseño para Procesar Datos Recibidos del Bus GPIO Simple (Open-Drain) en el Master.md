Diseño para Procesar Datos Recibidos del Bus GPIO Simple (Open-Drain) en el Master

**1. Recepción de la Señal en el Master (Raspberry Pi 5)**
El master escucha un bus GPIO compartido open-drain que está normalmente en estado HIGH gracias a una resistencia pull-up (preferiblemente externa, 10kΩ).
Cuando un nodo slave activa una interrupción, jala la línea a LOW (flanco descendente).
El master detecta este cambio inmediato mediante una interrupción hardware configurada para detectar FALLING_EDGE (transición de HIGH a LOW).
El uso del evento FALLING_EDGE garantiza máxima reactividad con baja latencia (microsegundos)

**2. Procesamiento de la Señal en el Master**
La interrupción genera una señal o flag que indica que ocurrió un evento.
El master comienza a medir la duración del pulso LOW (tiempo durante el cual la línea está en LOW), que corresponde al nodo que jala y suspende la línea según su tiempo único asignado.
Importante tiempo asignado del pulso low para cada nodo de forma distonta. 
Posteriormente cuando la linea vuelve a HIGH (flanco ascendente), verifica que el pulso haya durado al menos el umbral configurado para validar el evento.
Según la duración del pulso LOW, el master puede identificar qué nodo generó la interrupción, ya que cada nodo envía un pulso con distinta duración (por ejemplo, 100ms para nodo 1, 150ms para nodo 2, etc.).
El master realiza acciones consecuentes:
Notifica eventos críticos inmediatos (parada de emergencia, alarmas)
Consulta o intercambia datos mediante otro protocolo (ej: CANopen) con el nodo identificado para confirmar o extender la información.

**3. Coordinación en el Master**
El master puede tener un mecanismo para interrogar nodos vía CANopen u otro bus para confirmar eventos o manejar configuraciones.
Se organizan flags, logs y prioridades según la criticidad de la interrupción recibida.
 

**Manejo de Señal y API en el Slave**

1. Configuración GPIO en el Slave (ESP32)
El GPIO utilizado para este bus debe configurarse en modo open-drain para evitar conflictos eléctricos.
El pin debe:
Jalar la línea a LOW cuando el nodo detecta un evento crítico que requiera interrumpir.
Liberar la línea (modo entrada o estado flotante) cuando no esté activando la interrupción.
La duración del pulso LOW debe ser única o diferenciable para cada nodo (por ejemplo, nodo 1 mantiene LOW 100ms, nodo 2 150ms, etc.).

2. Lógica de la API en Slave (conceptual)
Un evento crítico detectado por un sensor, fallo o condición genera una función de activación de interrupción GPIO.
Esta función establece el pin GPIO en OUTPUT open-drain y lo pone a LOW.
Mantiene este estado por el tiempo único asignado.
Después vuelve a liberar el pin (modo entrada).
Opcionalmente, luego puede notificar o comunicarse usando un protocolo ,ej. CANopen) para enviar info detallada.

Función API básica conceptual:

void trigger_critical_interrupt() {
    config_pin_open_drain_output();
    set_pin_low();
    delay(pulse_duration_ms);
    set_pin_high_or_input();
    notify_master_event_occurred();  // Opcional, via CANopen
}



**3. Comunicación Secundaria Ejemplo (CANopen)**
Tras la señal física en GPIO, el slave puede enviar un mensaje CANopen con más datos.
Este protocolo permite comunicaciones fiables, sincronización y diagnósticos.
El master recibe y procesa estos datos junto con la señal física.

**Resumen Visual del Flujo**

[Slave nodo ESP32]
evento crítico detected
    ↓
jala bus GPIO a LOW (abajo) durante tiempo específico
    ↓
master detecta FALLING_EDGE en bus GPIO
    ↓
master mide duración del pulso LOW
    ↓
identifica nodo por duración del pulso
    ↓
master inicia proceso (alarma, parada, consulta CANopen)
    ↓
slave puede enviar detalles vía CANopen


**Caso de 2 o más nodos que envían al mismo tiempo (simultáneo)**
Cuando uno o varios nodos jalanean la línea a LOW simultáneamente, físicamente el bus simplemente estará LOW (pues open-drain hace un wired-OR).
Para el master, la línea aparece bajada a LOW sin distinción de cuántos ni cuáles nodos la mantienen en LOW.
El master recibe un único evento de flanco descendente (falling edge) indicando que "al menos un nodo activó la interrupción".


**El master detecta la interrupción (falling edge).**
Ejecuta la rutina rápida para registrar la interrupción y marca una bandera de evento.
Inicia una ronda de consulta o polling a cada nodo en la red:(CAN)
Los nodos responden según su estado. Así el master sabe exactamente quién generó evento(s).
Cuando ya no hay nodos activos con evento (todos respondieron no), el master da por atendida la señal del bus.
Protección Lógica (Software / Protocolo)
Ignora cambios en el bus que duren menos de cierto umbral (ejemplo 50 ms si sabes que el evento durará más).
Lo ideal es que el master haga lecturas repetidas rápidas del pin para asegurar que el bit está estable y no es un ruido momentáneo.
Protocolo de Paridad o Checksum
Aunque el bus es un solo bit, puedes implementar protocolos simples para validar el evento:
Por ejemplo, el slave podría enviar dos pulsos separados por un tiempo corto para indicar "1 válido".
El master solo acepta una señal si detecta ambos pulsos en secuencia.
Comunicación Paralela para Confirmación
Usa el bus GPIO para señalizar evento (1 bit)
Luego haz que master y slave se comuniquen por protocolo secundario (ej. CANopen, UART)
En esta comunicación secundaria, envía datos con chequeo de errores (CRC, checksums) para confirmar o corregir errores en la señal de interrupción.

