/* modelo_de_tareas.c - SEyTR, Unidad 3, sesión U3-S4.
 *
 * Qué hace este programa
 * ----------------------
 * Crea dos tareas de parpadeo con la misma función y parámetros distintos:
 * «rapido», que conmuta su LED cada 200 ms, y «lento», cada 750 ms. Ninguna
 * conoce a la otra. app_main las crea y termina, y el programa sigue.
 *
 * Desde el monitor, sin recompilar, se pueden ver las consecuencias de cada
 * parámetro de la creación y del ciclo de vida de una tarea:
 * - «i»: informe de cada tarea: estado, prioridad, núcleo y marca de agua de
 *   su pila, es decir, el mínimo de pila que le ha quedado libre, en bytes;
 * - «s»: pide a la tarea «rapido» que se suspenda, o la reanuda;
 * - «t»: pide a la tarea «lento» que termine de forma ordenada;
 * - «n»: crea una tercera tarea de parpadeo, «tercero», con la misma función,
 *   otro periodo y fijada al núcleo 1;
 * - «p»: crea una tarea con la pila insuficiente. Reinicia la placa, y por eso
 *   solo funciona si se activa en la configuración del proyecto;
 * - «v»: activa o desactiva las trazas de los parpadeos;
 * - «h»: la ayuda.
 *
 * Qué hardware necesita
 * ---------------------
 * La placa del curso, conectada por su conector USB-a-UART, que es por donde
 * llegan las teclas. Los LED son opcionales: sus pines se declaran en
 * pines.h, y sin ellos las trazas dicen cuándo conmuta cada tarea.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "led.h"
#include "pines.h"

static const char *TAG = "modelo_de_tareas";

/* Tamaños de pila, en bytes: en ESP-IDF la pila se da en bytes, no en palabras
 * como en el FreeRTOS original. Con «i» se ve cuánta queda libre. */
#define PILA_PARPADEO   3072
#define PILA_TECLADO    4096

/* Prioridades. Las tareas de parpadeo pasan casi todo el tiempo bloqueadas;
 * la del teclado, por debajo, solo atiende teclas. El criterio para elegir
 * prioridades es de U3-S5. */
#define PRIORIDAD_PARPADEO  5
#define PRIORIDAD_TECLADO   2

/* Los datos que recibe cada tarea de parpadeo por su parámetro.
 *
 * Son static: tienen que seguir existiendo mientras vivan las tareas. Si
 * fueran variables locales de app_main, dejarían de existir al terminar
 * app_main, y las tareas leerían memoria que ya no es suya. */
typedef struct {
    const char    *nombre;
    gpio_num_t     pin;
    uint32_t       periodo_ms;
    volatile bool *pedir_suspension;  /* NULL si la tarea no se suspende */
} parpadeo_t;

/* Petición de suspensión de «rapido». La pone a true la tarea del teclado y la
 * vuelve a false la propia «rapido» antes de suspenderse. Como s_trazas, es
 * una variable que comparten dos tareas: aquí basta, porque cada una solo la
 * cambia cuando la otra no lo va a hacer; compartir datos con garantías es
 * materia de la U4. */
static volatile bool s_rapido_pedir_suspension = false;

static const parpadeo_t s_rapido  = { .nombre = "rapido",  .pin = PIN_LED_RAPIDO,  .periodo_ms = 200,
                                      .pedir_suspension = &s_rapido_pedir_suspension };
static const parpadeo_t s_lento   = { .nombre = "lento",   .pin = PIN_LED_LENTO,   .periodo_ms = 750 };
static const parpadeo_t s_tercero = { .nombre = "tercero", .pin = PIN_LED_TERCERO, .periodo_ms = 330 };

/* Manejadores de las tareas. Los escribe quien crea cada tarea y solo los usa
 * la tarea del teclado. Cuando pide a «lento» que termine, la tarea del
 * teclado deja su manejador a NULL y no lo vuelve a usar: una vez borrada, la
 * tarea ya no existe y su manejador no vale. */
static TaskHandle_t s_tarea_rapido  = NULL;
static TaskHandle_t s_tarea_lento   = NULL;
static TaskHandle_t s_tarea_tercero = NULL;
static TaskHandle_t s_tarea_teclado = NULL;

static volatile bool s_trazas = true;

/* ------------------------------------------------------------------------
 * La función de las tareas de parpadeo. La comparten las tres: cada una la
 * ejecuta con sus propios datos, que le llegan por el parámetro, y con su
 * propia pila, donde viven sus variables locales.
 * ------------------------------------------------------------------------ */

static void tarea_parpadeo(void *argumento)
{
    const parpadeo_t *p = (const parpadeo_t *)argumento;

    led_t led;  /* variable local: está en la pila de esta tarea */
    if (led_iniciar(&led, p->pin, LED_NIVEL_ENCENDIDO) != ESP_OK) {
        /* Sin LED, la tarea sigue y sus trazas dicen cuándo conmuta. No se
         * borra: su manejador lo usa la tarea del teclado. */
        ESP_LOGE(TAG, "%s: no se ha podido iniciar el LED; sigue sin LED", p->nombre);
        (void)led_iniciar(&led, GPIO_NUM_NC, LED_NIVEL_ENCENDIDO);
    }
    printf("%s: empieza, conmuta cada %" PRIu32 " ms\n", p->nombre, p->periodo_ms);

    TickType_t referencia = xTaskGetTickCount();
    int64_t anterior = esp_timer_get_time();
    for (;;) {
        /* Espera absoluta, de U2-S1: el periodo no se desplaza con lo que
         * tarde el cuerpo del bucle. */
        const BaseType_t espero = xTaskDelayUntil(&referencia, pdMS_TO_TICKS(p->periodo_ms));

        /* ¿Le han pedido que termine? La petición llega como una notificación
         * a la tarea (U2-S2). Se mira sin esperar: el segundo argumento, 0,
         * es el tiempo máximo de espera. */
        if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            led_apagar(&led);  /* deja lo que maneja en un estado conocido */
            printf("%s: termina de forma ordenada y se borra a si misma\n", p->nombre);
            vTaskDelete(NULL); /* no vuelve */
        }

        led_conmutar(&led);
        const int64_t ahora = esp_timer_get_time();
        if (s_trazas) {
            const uint32_t us = (uint32_t)(ahora - anterior);
            printf("%s: conmuta, %" PRIu32 ",%" PRIu32 " ms desde la anterior%s\n",
                   p->nombre, us / 1000, (us % 1000) / 100,
                   espero == pdFALSE ? " (activacion tardia)" : "");
        }
        anterior = ahora;

        /* ¿Le han pedido que se suspenda? Se suspende a sí misma aquí, al
         * terminar una activación, cuando no tiene nada a medias. Si otra
         * tarea la suspendiera desde fuera, podría dejarla a mitad de un
         * printf(), con la consola tomada, y entonces ninguna otra tarea
         * podría escribir hasta reanudarla. */
        if (p->pedir_suspension != NULL && *p->pedir_suspension) {
            *p->pedir_suspension = false;
            printf("%s: se suspende\n", p->nombre);
            vTaskSuspend(NULL);  /* sigue aquí cuando otra tarea la reanude */
        }
    }
}

/* ------------------------------------------------------------------------
 * La tarea con la pila insuficiente («p»).
 * ------------------------------------------------------------------------ */

#if CONFIG_SEYTR_PILA_CORTA

#define PILA_CORTA  1536  /* bytes */
#define NIVELES_MAX 1000  /* muchos más de los que caben: no se llega */

/* Gasta pila poco a poco: cada nivel de la recursión ocupa un bloque local de
 * 64 bytes más el marco de la llamada, y se bloquea un tick antes de bajar al
 * siguiente. La pila crece hacia las direcciones bajas. FreeRTOS comprueba en
 * cada cambio de contexto unos bytes de control al principio de la zona de la
 * pila, la dirección más baja; cuando el nivel más profundo llega a ellos, los
 * encuentra cambiados y detiene el sistema con un mensaje que nombra la tarea.
 * Bajando de nivel en nivel, lo que se escribe fuera de la pila antes de la
 * comprobación queda acotado a lo que ocupa un nivel y el contexto guardado. */
static void bajar(uint32_t nivel)
{
    volatile uint8_t bloque[64];
    for (size_t i = 0; i < sizeof bloque; i++) {
        bloque[i] = (uint8_t)nivel;
    }
    vTaskDelay(1);  /* cambio de contexto: aquí se comprueba la pila */
    if (nivel < NIVELES_MAX) {
        bajar(nivel + 1);
    }
    bloque[0] = bloque[1];  /* usar el bloque después de la llamada impide
                             * que el compilador la convierta en un salto */
}

static void tarea_pila_corta(void *argumento)
{
    (void)argumento;
    bajar(0);
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

#endif

/* ------------------------------------------------------------------------
 * La tarea del teclado.
 * ------------------------------------------------------------------------ */

static const char *nombre_estado(eTaskState estado)
{
    switch (estado) {
    case eRunning:   return "en ejecucion";
    case eReady:     return "lista";
    case eBlocked:   return "bloqueada";
    case eSuspended: return "suspendida";
    case eDeleted:   return "borrada";
    default:         return "?";
    }
}

static void informar_tarea(const char *nombre, TaskHandle_t tarea)
{
    if (tarea == NULL) {
        printf("  %-8s sin manejador: no creada, o se le ha pedido que termine\n", nombre);
        return;
    }
    char nucleo[24];
    const BaseType_t afinidad = xTaskGetCoreID(tarea);
    if (afinidad == tskNO_AFFINITY) {
        snprintf(nucleo, sizeof nucleo, "sin afinidad");
    } else {
        snprintf(nucleo, sizeof nucleo, "nucleo %d", (int)afinidad);
    }
    printf("  %-8s %-12s prioridad %u  %-12s marca de agua %u bytes\n",
           nombre, nombre_estado(eTaskGetState(tarea)), (unsigned)uxTaskPriorityGet(tarea),
           nucleo, (unsigned)uxTaskGetStackHighWaterMark(tarea));
}

static void mostrar_ayuda(void)
{
    printf("\n=== SEyTR - modelo de tareas ===\n");
    printf("Teclas (sin Intro):\n");
    printf("  i  informe de las tareas: estado, prioridad, nucleo y marca de agua\n");
    printf("  s  pide a la tarea rapido que se suspenda, o la reanuda\n");
    printf("  t  pide a la tarea lento que termine de forma ordenada\n");
    printf("  n  crea la tarea tercero, fijada al nucleo 1\n");
#if CONFIG_SEYTR_PILA_CORTA
    printf("  p  crea una tarea con la pila insuficiente (reinicia la placa)\n");
#else
    printf("  p  desactivada: se activa en la configuracion del proyecto\n");
#endif
    printf("  v  trazas de los parpadeos si/no\n");
    printf("  h  esta ayuda\n\n");
}

static void crear_tercero(void)
{
    if (s_tarea_tercero != NULL) {
        printf("\nLa tarea tercero ya existe.\n");
        return;
    }
    /* La misma función con otros datos, y con el núcleo fijado: el último
     * argumento es la afinidad, 0, 1 o tskNO_AFFINITY. */
    if (xTaskCreatePinnedToCore(tarea_parpadeo, "tercero", PILA_PARPADEO, (void *)&s_tercero,
                                PRIORIDAD_PARPADEO, &s_tarea_tercero, 1) != pdPASS) {
        s_tarea_tercero = NULL;
        printf("\nNo hay memoria para la tarea tercero.\n");
    }
}

static void atender_tecla(char tecla)
{
    switch (tecla) {
    case 'i':
        printf("\nTareas del ejemplo:\n");
        informar_tarea("rapido", s_tarea_rapido);
        informar_tarea("lento", s_tarea_lento);
        informar_tarea("tercero", s_tarea_tercero);
        informar_tarea("teclado", s_tarea_teclado);
        break;
    case 's':
        if (s_tarea_rapido == NULL) {
            break;
        }
        if (eTaskGetState(s_tarea_rapido) == eSuspended) {
            printf("\nSe reanuda rapido.\n");
            vTaskResume(s_tarea_rapido);
        } else if (!s_rapido_pedir_suspension) {
            s_rapido_pedir_suspension = true;
            printf("\nPedido a rapido que se suspenda. Lo hara al terminar su proxima activacion.\n");
        } else {
            printf("\nYa se ha pedido a rapido que se suspenda.\n");
        }
        break;
    case 't':
        if (s_tarea_lento != NULL) {
            xTaskNotifyGive(s_tarea_lento);
            s_tarea_lento = NULL;  /* ya no se usa: la tarea va a borrarse */
            printf("\nPedido a lento que termine. Lo hara en su proxima activacion.\n");
        }
        break;
    case 'n':
        crear_tercero();
        break;
    case 'p':
#if CONFIG_SEYTR_PILA_CORTA
        printf("\nSe crea la tarea pila_corta, con %d bytes de pila.\n", PILA_CORTA);
        if (xTaskCreate(tarea_pila_corta, "pila_corta", PILA_CORTA, NULL,
                        PRIORIDAD_PARPADEO, NULL) != pdPASS) {
            printf("No hay memoria para la tarea pila_corta.\n");
        }
#else
        printf("\nLa tecla p esta desactivada: activa \"Permitir la tecla p\" en el menu\n"
               "\"SEyTR - Modelo de tareas\" de la configuracion, recompila y graba.\n");
#endif
        break;
    case 'v':
        s_trazas = !s_trazas;
        printf("\nTrazas de los parpadeos: %s\n", s_trazas ? "si" : "no");
        break;
    case 'h':
        mostrar_ayuda();
        break;
    default:
        break;
    }
}

static void tarea_teclado(void *argumento)
{
    (void)argumento;
    for (;;) {
        /* Sin el controlador de la UART instalado, read() sobre la consola no
         * espera: devuelve -1 enseguida si no ha llegado nada. */
        char tecla;
        if (read(STDIN_FILENO, &tecla, 1) == 1) {
            atender_tecla(tecla);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "SEyTR - modelo de tareas. Tick a %d Hz", configTICK_RATE_HZ);
    mostrar_ayuda();

    /* xTaskCreate(función, nombre, pila en bytes, parámetro, prioridad,
     * manejador). Devuelve pdPASS si la tarea se ha creado; si no hay memoria
     * para su pila y su bloque de control, no se crea, y hay que saberlo. En
     * ESP-IDF, xTaskCreate() crea la tarea sin afinidad: puede ejecutarse en
     * cualquiera de los dos núcleos. */
    if (xTaskCreate(tarea_parpadeo, "rapido", PILA_PARPADEO, (void *)&s_rapido,
                    PRIORIDAD_PARPADEO, &s_tarea_rapido) != pdPASS) {
        s_tarea_rapido = NULL;
        ESP_LOGE(TAG, "no hay memoria para la tarea rapido");
    }
    if (xTaskCreate(tarea_parpadeo, "lento", PILA_PARPADEO, (void *)&s_lento,
                    PRIORIDAD_PARPADEO, &s_tarea_lento) != pdPASS) {
        s_tarea_lento = NULL;
        ESP_LOGE(TAG, "no hay memoria para la tarea lento");
    }
    if (xTaskCreate(tarea_teclado, "teclado", PILA_TECLADO, NULL,
                    PRIORIDAD_TECLADO, &s_tarea_teclado) != pdPASS) {
        s_tarea_teclado = NULL;
        ESP_LOGE(TAG, "no hay memoria para la tarea del teclado");
    }

    ESP_LOGI(TAG, "tareas creadas; app_main termina y las tareas siguen");
}
