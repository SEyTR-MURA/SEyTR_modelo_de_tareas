# SEyTR · El modelo de tareas

Proyecto de ejemplo de **Sistemas Empotrados y de Tiempo Real**. Es el de la
sesión **U3-S4**, creación de tareas. Crea dos tareas de parpadeo con la misma
función y parámetros distintos, y permite ver desde el monitor, sin
recompilar, las consecuencias de cada parámetro de la creación y del ciclo de
vida de una tarea: la pila y su marca de agua, la suspensión, la terminación
ordenada, la afinidad de núcleo y el desbordamiento de pila.

Reutiliza el componente `led` de `SEyTR_entrada_por_interrupcion`, el ejemplo
de U2-S2, sin cambios.

No hace falta cuenta ni permisos para clonarlo: el repositorio es público.

## Qué hace

Dos tareas, `rapido` y `lento`, ejecutan la misma función, `tarea_parpadeo()`.
Cada una recibe sus datos por el parámetro de la tarea: su nombre, el pin de
su LED y su periodo, 200 ms y 750 ms. Cada vez que despierta, conmuta su LED y
escribe cuánto ha pasado desde la vez anterior. Ninguna conoce a la otra.
`app_main` las crea con `xTaskCreate()` y termina, y el programa sigue.

Una tercera tarea, la del teclado, atiende las teclas del monitor.

## Qué necesitas antes

El entorno instalado y verificado con la guía de instalación de la asignatura,
y la placa con su cable USB de datos, conectado al conector **USB-a-UART** de
la placa, que es por donde llegan las teclas al programa. Los tres LED, con su
resistencia, son **opcionales**: sin ellos, las trazas dicen cuándo conmuta
cada tarea.

## Cómo obtenerlo

Clona el repositorio en una carpeta de tu equipo con una ruta **corta y sin
espacios ni tildes**, y abre en el editor la carpeta que contiene el
`CMakeLists.txt` de la raíz. Compila, graba y abre el monitor desde la
extensión de ESP-IDF, como en las sesiones anteriores. Si prefieres la línea
de órdenes, `idf.py build flash monitor` hace lo mismo.

## Antes de compilar: los pines

Los pines se declaran en un solo sitio, `main/pines.h`, y el repositorio llega
**sin pines elegidos**. Se eligen como en U2-S1 y se anotan en esta tabla:

| Señal | Constante | GPIO | Montaje |
| --- | --- | --- | --- |
| LED de `rapido` | `PIN_LED_RAPIDO` | *sin elegir* | Entre el pin y masa, con su resistencia en serie |
| LED de `lento` | `PIN_LED_LENTO` | *sin elegir* | Entre el pin y masa, con su resistencia en serie |
| LED de `tercero` | `PIN_LED_TERCERO` | *sin elegir* | Entre el pin y masa, con su resistencia en serie |

Si un pin no se puede usar como salida, la tarea lo dice y sigue sin LED.

## Cómo se usa

Las teclas se pulsan sin Intro.

| Tecla | Qué hace |
| --- | --- |
| `i` | Informe de cada tarea: estado, prioridad, núcleo y marca de agua de su pila |
| `s` | Pide a la tarea `rapido` que se suspenda, o la reanuda si está suspendida |
| `t` | Pide a la tarea `lento` que termine de forma ordenada |
| `n` | Crea la tarea `tercero`, con la misma función, periodo de 330 ms y fijada al núcleo 1 |
| `p` | Crea una tarea con la pila insuficiente. **Reinicia la placa**; viene desactivada |
| `v` | Activa o desactiva las trazas de los parpadeos |
| `h` | Vuelve a imprimir la lista |

**La marca de agua** de una tarea es el mínimo de pila que le ha quedado libre
desde que se creó, en bytes. La da `uxTaskGetStackHighWaterMark()`.

**La suspensión.** La tecla `s` no suspende a `rapido` desde fuera: le pide
que se suspenda, y `rapido` lo hace ella misma con `vTaskSuspend(NULL)` al
terminar su siguiente activación. Si la tarea del teclado la suspendiera con
`vTaskSuspend()` en un instante cualquiera, podría dejarla a mitad de escribir
una traza, con la consola tomada: ninguna otra tarea podría escribir hasta
reanudarla, y el programa parecería colgado. Para reanudarla, la tarea del
teclado llama a `vTaskResume()`, que sí se hace desde fuera.

**La terminación ordenada.** La tecla `t` envía una notificación a `lento`,
como las de U2-S2. La tarea la mira cada vez que despierta: si la encuentra,
apaga su LED, lo dice y se borra a sí misma con `vTaskDelete(NULL)`.

**Activar la tecla `p`.** En el editor de la configuración del proyecto
(*SDK Configuration Editor* de la extensión, o `idf.py menuconfig`), menú
«SEyTR - Modelo de tareas», opción «Permitir la tecla p». Recompila y graba.

## Qué deberías ver

Para cada tecla se describe lo que debe pasar y, después de «Medido:», las
cifras obtenidas en una placa del curso. Tus cifras pueden variar algo.

- **Al arrancar.** Un aviso del componente `led` por cada LED sin pin elegido,
  las dos tareas que empiezan y la línea `main_task: Returned from app_main()`,
  que confirma que `app_main` ha terminado. Las trazas de `rapido` dicen unos
  200 ms entre conmutaciones, y las de `lento`, unos 750 ms. Medido: entre
  199,9 y 200,0 ms, y entre 749,9 y 750,0 ms; la primera, a veces algo
  menos.
- **`i`.** Las dos tareas de parpadeo, «bloqueada» casi siempre, porque pasan
  casi todo el tiempo esperando; prioridad 5; «sin afinidad», porque
  `xTaskCreate()` crea sin fijar el núcleo; y su marca de agua. La tarea del
  teclado sale «en ejecucion», porque es ella la que escribe el informe.
  Medido: marca de agua de 1832 y 1836 bytes en `rapido` y `lento`, de los
  3072 de su pila, y de 2896 en la del teclado, de 4096; las mismas en `rapido` y en el teclado al
  cabo de más de un minuto.
- **`s`.** `rapido` escribe que se suspende en su siguiente activación, deja de
  conmutar y el informe la da «suspendida». Al reanudarla, recupera de golpe
  las activaciones perdidas: su espera absoluta tenía instantes ya pasados, y
  `xTaskDelayUntil()` vuelve enseguida en cada uno, marcado como «activacion
  tardia», hasta alcanzar el siguiente instante futuro. La primera traza da el
  tiempo que ha estado suspendida; tras 3 s suspendida, salen unas quince
  tardías. Si se reanuda antes de 200 ms, no hay ninguna. Si se pulsa `s` dos
  veces antes de que se suspenda, la segunda responde «Ya se ha pedido a rapido
  que se suspenda.». Las tardías no salen
  todas a la vez: cada vuelta escribe su traza, y `printf()` espera, sin
  bloquearse, a que quepa en la memoria de la UART de la consola, de
  128 bytes. Una traza de 62 caracteres, a 115200 baudios y 10 bits por
  carácter, tarda 5,4 ms en salir, y ese es el intervalo entre tardías en
  cuanto esa memoria se llena.
  Escribir también ocupa tiempo de la tarea. Medido: tras 4,6 s suspendida,
  23 tardías: la primera a 4605,1 ms de la anterior, las dos siguientes a 0,1
  y 1,7 ms, y las demás a 5,4 ms, salvo una de 9,2 ms; después, vuelta a 200 ms.
- **`t`.** En su siguiente activación, `lento` escribe que termina y se borra.
  El informe deja de darla en cuanto se pulsa `t`, antes de que termine: la
  tarea del teclado suelta su manejador al pedírselo, porque después de que la
  tarea se borre el manejador ya no vale. Medido: «lento: termina de forma
  ordenada y se borra a si misma», y el informe la da «sin manejador».
- **`n`.** `tercero` empieza a conmutar cada 330 ms y el informe la da fijada
  al «nucleo 1». Medido: entre 329,9 y 330,0 ms. Si el pin de su LED no
  existe, avisa con un error, sigue sin LED y conmuta igual.
- **`p`**, si está activada. La tarea `pila_corta` llama a una función que se
  llama a sí misma, y cada nivel ocupa un poco más de pila y se bloquea un
  tick. Al cabo de unos pocos niveles se sale de su pila: FreeRTOS lo detecta
  en un cambio de contexto, escribe `***ERROR*** A stack overflow in task
  pila_corta has been detected.` con la traza de llamadas, que puede acabar en
  `|<-CORRUPTED`, y la placa se reinicia. La comprobación solo se hace en los
  cambios de contexto: cuando se detecta, ya se ha escrito memoria fuera de la
  pila. Y no está garantizada: el método que viene activado mira unos bytes de
  control al final de la pila, y el libro de FreeRTOS advierte de que es muy
  improbable, pero posible, que un desbordamiento no los toque. Medido: el
  mensaje sale enseguida, la traza pasa por `vApplicationStackOverflowHook` y
  `vTaskSwitchContext`, es decir, por el cambio de contexto, y acaba en
  `|<-CORRUPTED`; la placa se reinicia (`rst:0xc (RTC_SW_CPU_RST)`) y vuelve a
  arrancar normal. Igual recién arrancada, con `lento` terminada y con
  `tercero` creada.

## Qué puedes cambiar sin tocar el código

El proyecto fija en `sdkconfig.defaults` el destino `esp32s3` y el tick a
1000 Hz. Si en tu `sdkconfig` hay otro valor fijado a mano, manda sobre
`sdkconfig.defaults`: cámbialo en la configuración o borra `sdkconfig` y
vuelve a fijar el destino.

## Estructura

```
CMakeLists.txt          define el proyecto
sdkconfig.defaults      configuración de partida; se escribe a mano y se versiona
main/
  CMakeLists.txt        registra las fuentes del componente principal
  Kconfig.projbuild     la opción que permite la tecla p
  pines.h               los pines y su montaje, en un solo sitio
  modelo_de_tareas.c    app_main, la función de las tareas de parpadeo y el teclado
components/
  led/                  componente propio: el LED, el de U2-S1 y U2-S2
```

No se versionan el fichero `sdkconfig`, que genera la herramienta a partir de
`sdkconfig.defaults`, ni la carpeta `build/`.

La pila de cada tarea se da en **bytes**, como pide ESP-IDF, y no en palabras,
como en el FreeRTOS original. Los datos de cada tarea son variables `static`:
tienen que seguir existiendo mientras viva la tarea, y una variable local de
`app_main` deja de existir cuando `app_main` termina.

## Fuentes

- Espressif, guía de ESP-IDF v6.1 para el ESP32-S3,
  [FreeRTOS (IDF)](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/freertos_idf.html),
  apartado «Tasks»: creación, pila en bytes, afinidad y borrado.
- Espressif, guía de ESP-IDF v6.1,
  [uso de la RAM](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-guides/performance/ram-usage.html):
  la marca de agua, en bytes.
- Espressif, guía de ESP-IDF v6.1,
  [errores fatales](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-guides/fatal-errors.html):
  las comprobaciones de desbordamiento de pila.
- Espressif, guía de ESP-IDF v6.1,
  [arranque de la aplicación](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-guides/startup.html):
  qué pasa cuando `app_main` termina.
- Espressif, ESP-IDF v6.1, código fuente: velocidad de la consola por omisión,
  115200 baudios ([`Kconfig` de `esp_stdio`](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_stdio/Kconfig));
  memoria de transmisión de la UART del ESP32-S3, 128 bytes
  ([`soc_caps.h`](https://github.com/espressif/esp-idf/blob/v6.1/components/soc/esp32s3/include/soc/soc_caps.h));
  y espera activa de la consola mientras esa memoria está llena
  ([`uart_vfs.c`](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_driver_uart/src/uart_vfs.c)).
- R. Barry, [*Mastering the FreeRTOS Real Time Kernel*](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book),
  capítulo 4, gestión de tareas, y apartado 13.3, desbordamiento de pila.

## Un aviso

Este repositorio es material de la asignatura. El repositorio de tu **proyecto
final** es otro distinto, con su propio nombre y sus propias reglas.
