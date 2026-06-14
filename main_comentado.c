/*
 * Informatica II
 * Laboratorio III: Uso de semáforos y pipes entre procesos en Linux
 * Repositorio: <https://github.com/smarc05/Laboratorio-III.git>
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
    sem_t semaforo; //  sem_t es una variable diseñada específicamente por el sistema operativo para actuar como un semáforo.
} MemoriaCompartida;

// Declaramos el puntero a la memoria compartida como global para que las
// funciones credito() y debito() puedan acceder al saldo y al semáforo.
MemoriaCompartida *memoria_compartida = NULL;

// ----------------------------------------------------------------------------
// FUNCIÓN DEL PROCESO HIJO CRÉDITO
// ----------------------------------------------------------------------------
void credito(char *archivo_montos, int p[])
{
    // El hijo solo escribe, así que cerramos el extremo de lectura del pipe
    close(p[0]);

    FILE *archivo = fopen(archivo_montos, "r");
    if (archivo == NULL)
    {
        perror("Error al abrir el archivo de Credito");
        exit(EXIT_FAILURE);
    }

    int monto;
    while (fscanf(archivo, "%d", &monto) == 1)
    {
        // --- ENTRADA A LA REGIÓN CRÍTICA ---
        sem_wait(&memoria_compartida->semaforo);
        memoria_compartida->saldo += monto;
        sem_post(&memoria_compartida->semaforo);
        // --- SALIDA DE LA REGIÓN CRÍTICA ---

        // Enviamos el monto al padre por el extremo de escritura del pipe
        write(p[1], &monto, sizeof(int));
    }

    fclose(archivo);

    // Antes de finalizar deben cerrar el pipe que estaban usando
    close(p[1]);
}

// ----------------------------------------------------------------------------
// FUNCIÓN DEL PROCESO HIJO DÉBITO
// ----------------------------------------------------------------------------
void debito(char *archivo_montos, int p[])
{
    // El hijo solo escribe, cerramos el extremo de lectura
    close(p[0]);

    FILE *archivo = fopen(archivo_montos, "r");
    if (archivo == NULL)
    {
        perror("Error al abrir el archivo de Debito");
        exit(EXIT_FAILURE);
    }

    int monto;
    while (fscanf(archivo, "%d", &monto) == 1)
    {
        // --- ENTRADA A LA REGIÓN CRÍTICA ---
        sem_wait(&memoria_compartida->semaforo);
        memoria_compartida->saldo -= monto;
        sem_post(&memoria_compartida->semaforo);
        // --- SALIDA DE LA REGIÓN CRÍTICA ---

        // Enviamos el monto al padre por el pipe
        write(p[1], &monto, sizeof(int));
    }

    fclose(archivo);

    // Cerramos el pipe al terminar
    close(p[1]);
}

int main()
{
    //  mmap es una función que le pide a Linux que reserve un bloque de memoria RAM y que nos permiten configurar esa RAM para que varios procesos distintos la puedan ver y compartir.
    //  PROT_READ | PROT_WRITE son los permisos para poder leer y escribir en el bloque de memoria pedido.
    //  MAP_SHARED le dice que cualquier cambio que haga un proceso en el bloque de memoria, debe ser visible automáticamente para los demás procesos.
    //  MAP_ANONYMOUS le dice: "No voy a usar ningún archivo físico, solo dame un bloque de memoria RAM en blanco".
    //  El -1 es el "Descriptor de Archivo". Como no hay ningún archivo de texto o base de datos conectada a esta memoria, le pasamos -1 para confirmar que no hay archivo.
    //  El 0 es el "Desplazamiento". Si estuvieras leyendo un archivo, esto le dice desde qué byte empezar a leer. Como no hay archivo, arranca en 0.

    // LINEA CORREGIDA: Se usa la variable global en minúsculas y el tamaño correcto de la estructura en sizeof
    memoria_compartida = mmap(NULL, sizeof(MemoriaCompartida), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memoria_compartida == MAP_FAILED) // Nota: mmap devuelve MAP_FAILED en caso de error
    {
        perror("Error al crear la memoria compartida.\n");
        exit(EXIT_FAILURE);
    }

    memoria_compartida->saldo = 0; //  Inicializamos el saldo en 0.

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
    // CREACIÓN DE PIPES (Completando Paso 1)
    // ========================================================================
    //  Un pipe (tubería) es un mecanismo de comunicación unidireccional.
    //  Necesita un array de 2 enteros:
    //  - posicion [0]: es el extremo de LECTURA (por donde el padre va a leer).
    //  - posicion [1]: es el extremo de ESCRITURA (por donde el hijo va a escribir).

    int pipe_credito[2];
    int pipe_debito[2];

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
        // Llamamos a la función encargada de procesar el archivo de crédito
        credito("credito.txt", pipe_credito);

        // Una vez que la función termina de procesar todo, el hijo muere limpiamente
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
        // CÓDIGO DEL HIJO DE DÉBITO
        // --------------------------------------------------------------------
        // Llamamos a la función encargada de procesar el archivo de débito
        debito("debito.txt", pipe_debito);

        // Una vez que la función termina de procesar todo, el hijo muere limpiamente
        exit(EXIT_SUCCESS);
    }

    // ------------------------------------------------------------------------
    // CONTINUACIÓN DEL PROCESO PADRE (Bucle de lectura)
    // ------------------------------------------------------------------------
    printf("[Padre] Esperando transacciones de los hijos...\n");

    // El padre solo lee, así que cerramos los extremos de escritura
    close(pipe_credito[1]);
    close(pipe_debito[1]);

    int monto_leido;
    // Variables para guardar la cantidad de bytes que lee read()
    // Si read() devuelve 0, significa que el hijo cerró su pipe
    int bytes_credito = 1;
    int bytes_debito = 1;

    // Mientras alguno de los dos pipes siga abierto (devolviendo > 0 bytes)
    while (bytes_credito > 0 || bytes_debito > 0)
    {

        // Intentamos leer del pipe de crédito
        if (bytes_credito > 0)
        {
            bytes_credito = read(pipe_credito[0], &monto_leido, sizeof(int));
            if (bytes_credito > 0)
            {
                printf("[Padre] Crédito reporta: +$%d\n", monto_leido);
            }
        }

        // Intentamos leer del pipe de débito
        if (bytes_debito > 0)
        {
            bytes_debito = read(pipe_debito[0], &monto_leido, sizeof(int));
            if (bytes_debito > 0)
            {
                printf("[Padre] Débito reporta:  -$%d\n", monto_leido);
            }
        }
    }

    // Esperamos a que los procesos hijos terminen de morir por completo
    wait(NULL);
    wait(NULL);

    // Mostramos el saldo final como pide la guía
    printf("\n====================================\n");
    printf("[Padre] EL SALDO FINAL ES: $%d\n", memoria_compartida->saldo);
    printf("====================================\n");

    // ========================================================================
    // LIMPIEZA FINAL DE RECURSOS (Movida al final real del programa)
    // ========================================================================
    close(pipe_credito[0]);
    close(pipe_debito[0]);
    sem_destroy(&memoria_compartida->semaforo);
    munmap(memoria_compartida, sizeof(MemoriaCompartida));

    printf("[Padre] Recursos liberados y programa finalizado con éxito.\n");

    return 0;
}