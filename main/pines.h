/* pines.h - los pines del proyecto, en un solo sitio.
 *
 * Cada número de pin se escribe aquí y en ningún otro fichero de código. El
 * componente led lo recibe como parámetro al iniciarse, y la tabla de pines
 * del README tiene que decir lo mismo que este fichero.
 */
#pragma once

#include "driver/gpio.h"

/* Pines de los LED de las tres tareas de parpadeo.
 *
 * GPIO_NUM_NC significa «sin conectar»: el repositorio llega así a propósito,
 * porque los pines los eliges tú. Los LED son opcionales: sin ellos, cada
 * tarea escribe en el monitor cuándo conmuta y con qué periodo.
 *
 * Se eligen como en U2-S1: con el esquema de tu placa y la tabla de pines de
 * la hoja de características del ESP32-S3, descartando los que sirven a la
 * memoria externa, los de arranque, los del USB y la consola y los que la
 * placa ya usa. Después escribe aquí su número, por ejemplo GPIO_NUM_xx,
 * compila y graba.
 */
#define PIN_LED_RAPIDO          GPIO_NUM_NC
#define PIN_LED_LENTO           GPIO_NUM_NC
#define PIN_LED_TERCERO         GPIO_NUM_NC

/* Nivel lógico que enciende los LED. Lo decide el montaje, no el código:
 *   1 si el LED y su resistencia van entre el pin y masa;
 *   0 si van entre el pin y 3,3 V. */
#define LED_NIVEL_ENCENDIDO     1
