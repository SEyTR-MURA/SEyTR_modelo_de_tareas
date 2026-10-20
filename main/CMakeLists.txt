# CMakeLists.txt de componente. Registra las fuentes del programa.
# PRIV_REQUIRES dice de que componentes depende: el LED del propio proyecto,
# y esp_timer, que da el reloj en microsegundos con el que se mide el periodo.
idf_component_register(SRCS "modelo_de_tareas.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES led esp_timer)
