#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"

// ---------------------------------------------------
// Configuración de pines
// ---------------------------------------------------
#define OUTPUT_GPIO 19   // línea compartida
#define BUTTON_GPIO 0    // botón BOOT

// ---------------------------------------------------
// Parámetros ajustables
// ---------------------------------------------------
#define PULSE_WIDTH_MS 200   // duración del pulso (ms)
#define RETRY_DELAY_MS 3000  // demora entre reintentos (ms)
#define MAX_RETRIES 50       // número máximo de intentos

// ---------------------------------------------------
static const char *TAG = "pulse_generator";
static QueueHandle_t gpio_evt_queue = NULL;
static bool pulse_active = false;
static uint32_t pulse_start_time = 0;

// ---------------------------------------------------
// ISR del botón
// ---------------------------------------------------
static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// ---------------------------------------------------
// Tarea que maneja interrupciones del botón
// ---------------------------------------------------
static void gpio_task(void *arg) {
    uint32_t io_num;
    static uint32_t last_press_time = 0;

    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

            // Debounce: ignora pulsos muy seguidos
            if ((now - last_press_time) < 150) {
                continue;
            }
            last_press_time = now;

            // Confirmar que el botón sigue presionado (nivel bajo)
            if (gpio_get_level(BUTTON_GPIO) == 0) {
                ESP_LOGI(TAG, "Interrupción - Botón presionado");

                if (!pulse_active) {
                    int attempts = 0;

                    // Esperar a que la línea esté libre (HIGH)
                    while (gpio_get_level(OUTPUT_GPIO) == 0) {
                        ESP_LOGW(TAG, "Línea ocupada (LOW), intento %d...", ++attempts);
                        if (attempts >= MAX_RETRIES) {
                            ESP_LOGE(TAG, "Timeout: línea sigue ocupada demasiado tiempo");
                            break;
                        }
                        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                    }

                    // Si la línea está libre (HIGH)
                    if (gpio_get_level(OUTPUT_GPIO) == 1) {
                        pulse_active = true;
                        pulse_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

                        gpio_set_level(OUTPUT_GPIO, 0);  // LÍNEA A LOW   
                        ESP_LOGI(TAG, "Pulso iniciando: línea LOW por %d ms", PULSE_WIDTH_MS);
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------
// Configuración de GPIOs
// ---------------------------------------------------
void setup_gpio(void)
{
    // **Restablecer el pin por completo** por si estaba usado por otro periférico
    gpio_reset_pin(OUTPUT_GPIO);

    // Configurar pin de salida (línea compartida)
    gpio_config_t output_config = {
    .pin_bit_mask = (1ULL << OUTPUT_GPIO),
    .mode = GPIO_MODE_INPUT_OUTPUT_OD,   // Permitir lectura desde salida
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&output_config);

    gpio_set_level(OUTPUT_GPIO, 1);
    int level = gpio_get_level(OUTPUT_GPIO);
    ESP_LOGI(TAG, "Nivel lógico GPIO %d: %d", OUTPUT_GPIO, level);

    // ---------------------------------------------------
    // Configurar botón (input)
    // ---------------------------------------------------
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE // flanco descendente
    };
    gpio_config(&button_config);

    // ---------------------------------------------------
    // Inicialización de ISR y cola
    // ---------------------------------------------------
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);
}

// ---------------------------------------------------
// Actualiza fin de pulso
// ---------------------------------------------------
void update_pulse(void) {
    if (pulse_active) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if ((now - pulse_start_time) >= PULSE_WIDTH_MS) {
            gpio_set_level(OUTPUT_GPIO, 1);  // Vuelve a HIGH
            pulse_active = false;
            ESP_LOGI(TAG, "Pulso completado: línea HIGH/restaurada");
        }
    }
}

// ---------------------------------------------------
// Función principal
// ---------------------------------------------------
void app_main(void) {
    setup_gpio();

    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Sistema en modo 'HIGH=activo / LOW=pulso'");
    ESP_LOGI(TAG, "Presiona BOOT para enviar un pulso LOW de %d ms", PULSE_WIDTH_MS);

    while (1) {
        update_pulse();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
