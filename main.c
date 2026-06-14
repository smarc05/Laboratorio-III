/*
 * Informatica II [cite: 1]
 * Laboratorio III: Uso de semáforos y pipes entre procesos en Linux [cite: 4]
 * Repositorio: <https://github.com/smarc05/Laboratorio-III.git> [cite: 7]
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>

typedef struct
{
    int saldo;
    [cite:13, 36] sem_t semaforo; //  sem_t es una variable diseñada específicamente por el sistema operativo para actuar como un semáforo. [cite: 36]
} MemoriaCompartida;

int main()
{
    //  mmap es una función que le pide a Linux que reserve un bloque de memoria RAM y que nos permiten configurar esa RAM para que varios procesos distintos la puedan ver y compartir.
    //  PROT_READ | PROT_WRITE son los permisos para poder leer y escribir en el bloque de memoria pedido.
    //  MAP_SHARED le dice que cualquier cambio que haga un proceso en el bloque de memoria, debe ser visible automáticamente para los demás procesos.
    //  MAP_ANONYMOUS le dice: "No voy a usar ningún archivo físico, solo dame un bloque de memoria RAM en blanco".
    //  El -1 es el "Descriptor de Archivo". Como no hay ningún archivo de texto o base de datos conectada a esta memoria, le pasamos -1 para confirmar que no hay archivo.
    //  El 0 es el "Desplazamiento". Si estuvieras leyendo un archivo, esto le dice desde qué byte empezar a leer. Como no hay archivo, arranca en 0.

    MemoriaCompartida *memoria_compartida = mmap(NULL, sizeof(MemoriaCompartida), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memoria_compartida == MAP_FAILED) // Nota: mmap devuelve MAP_FAILED en caso de error
    {
        perror("Error al crear la memoria compartida.\n");
        exit(EXIT_FAILURE);
    }

    memoria_compartida->saldo = 0; //  Inicializamos el saldo en 0. [cite: 13]

    //  La función sem_init() "enciende" el semáforo y le da sus valores iniciales. Si logra hacerlo bien devuelve un 0, pero si falla por algún motivo, devuelve un -1.
    //  El primer 1 le avisa que este semáforo se va a compartir entre procesos distintos.
    //  El segundo 1 es el valor inicial del semáforo.

    if (sem_init(&memoria_compartida->semaforo, 1, 1) < 0)
    {
        perror("Error al inicializar el semaforo.\n");
        munmap(memoria_compartida, sizeof(MemoriaCompartida));
        exit(EXIT_FAILURE);
    }

    // ========================================================================
    // NUEVO: CREACIÓN DE PIPES (Completando Paso 1)
    // ========================================================================
    //  Un pipe (tubería) es un mecanismo de comunicación unidireccional.
    //  Necesita un array de 2 enteros:
    //  - posicion [0]: es el extremo de LECTURA (por donde el padre va a leer).
    //  - posicion [1]: es el extremo de ESCRITURA (por donde el hijo va a escribir). [cite: 34]

    int pipe_credito[2];
    [cite:31, 34] int pipe_debito[2];
    [cite:30, 34]

        // Creamos el pipe para el proceso de Crédito
        if (pipe(pipe_credito) < 0)
    {
        perror("Error al crear el pipe de Credito.\n");
        sem_destroy(&memoria_compartida->semaforo);
        munmap(memoria_compartida, sizeof(MemoriaCompartida));
        exit(EXIT_FAILURE);
    }

    // Creamos el pipe para el proceso de Débito
    if (pipe(pipe_debito) < 0)
    {
        perror("Error al crear el pipe de Debito.\n");
        close(pipe_credito[0]);
        close(pipe_credito[1]);
        sem_destroy(&memoria_compartida->semaforo);
        munmap(memoria_compartida, sizeof(MemoriaCompartida));
        exit(EXIT_FAILURE);
    }

    printf("Parte 1 Completada con éxito: Memoria compartida, semáforo y pipes listos.\n");
    // ====================================================
    // PASO 2: CREACIÓN DE LOS PROCESOS HIJOS (fork)
    // ================================================
    pid_t pid_credito, pid_debito;

    // 1. Clonamos el proceso padre para crear al primer hijo (Crédito)
    pid_credito = fork();

    if (pid_credito < 0)
    {
        perror("Error al crear el hijo de Credito.\n");
        sem_destroy(&memoria_compartida->semaforo);
        munmap(memoria_compartida, sizeof(MemoriaCompartida));
        exit(EXIT_FAILURE);
    }
    else if (pid_credito == 0)
    {
        // --------------------------------------------------------------------
        // CÓDIGO DEL HIJO CRÉDITO
        // --------------------------------------------------------------------
        printf("[Hijo Credito] Proceso creado correctamente.\n");

        // En los próximos pasos llamaremos a la función credito() aquí.
        // Por ahora, simulamos que termina de inmediato de forma limpia.
        exit(EXIT_SUCCESS);
    }

    // Si el flujo sigue por acá, significa que somos el PADRE.
    // Ahora el padre clona un segundo hijo (Débito)
    pid_debito = fork();

    if (pid_debito < 0)
    {
        perror("Error al crear el hijo de Debito.\n");
        sem_destroy(&memoria_compartida->semaforo);
        munmap(memoria_compartida, sizeof(MemoriaCompartida));
        exit(EXIT_FAILURE);
    }
    else if (pid_debito == 0)
    {
        // --------------------------------------------------------------------
        // CÓDIGO DEL HIJO DÉBITO
        // --------------------------------------------------------------------
        printf("[Hijo Debito] Proceso creado correctamente.\n");

        // En los próximos pasos llamaremos a la función debito() aquí.
        // Por ahora, simulamos que termina de inmediato de forma limpia.
        exit(EXIT_SUCCESS);
    }

    // ------------------------------------------------------------------------
    // CONTINUACIÓN DEL PROCESO PADRE
    // ------------------------------------------------------------------------
    printf("[Padre] Esperando que los hijos terminen su trabajo...\n");

    // El padre debe esperar a que terminen ambos hijos para que no queden
    // como "procesos zombi" en el sistema operativo.
    wait(NULL);
    wait(NULL);

    // ========================================================================
    // LIMPIEZA FINAL DE RECURSOS (Movida al final real del programa)
    // ========================================================================
    close(pipe_credito[0]);
    close(pipe_credito[1]);
    close(pipe_debito[0]);
    close(pipe_debito[1]);
    sem_destroy(&memoria_compartida->semaforo);
    munmap(memoria_compartida, sizeof(MemoriaCompartida));

    printf("[Padre] Recursos liberados y programa finalizado con éxito.\n");

    return 0;
}