/*
 * Informatica II
 * Laboratorio III: Uso de semáforos y pipes entre procesos en Linux
 * Repositorio: https://github.com/smarc05/Laboratorio-III.git
 * Autores: Ortigosa Laureano, Salas Marcos.
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
    float saldo;
    sem_t semaforo;
} MemoriaCompartida;

MemoriaCompartida *memoria_compartida = NULL;

void credito(char *archivo_montos, int p[]) // FUNCIÓN DEL PROCESO HIJO CRÉDITO
{
    close(p[0]);

    FILE *archivo = fopen(archivo_montos, "r");
    if (archivo == NULL)
    {
        perror("Error al abrir el archivo de credito");
        exit(EXIT_FAILURE);
    }

    float monto;
    while (fscanf(archivo, "%f", &monto) == 1)
    {
        sem_wait(&memoria_compartida->semaforo);
        memoria_compartida->saldo += monto;
        sem_post(&memoria_compartida->semaforo);

        write(p[1], &monto, sizeof(float));
    }

    fclose(archivo);
    close(p[1]);
}

void debito(char *archivo_montos, int p[]) // FUNCIÓN DEL PROCESO HIJO DEBITO
{
    close(p[0]);

    FILE *archivo = fopen(archivo_montos, "r");
    if (archivo == NULL)
    {
        perror("Error al abrir el archivo de debito");
        exit(EXIT_FAILURE);
    }

    float monto;
    while (fscanf(archivo, "%f", &monto) == 1)
    {
        sem_wait(&memoria_compartida->semaforo);
        memoria_compartida->saldo -= monto;
        sem_post(&memoria_compartida->semaforo);

        write(p[1], &monto, sizeof(float));
    }

    fclose(archivo);
    close(p[1]);
}

int main()
{
    memoria_compartida = mmap(NULL, sizeof(MemoriaCompartida), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memoria_compartida == MAP_FAILED)
    {
        perror("Error al crear la memoria compartida.\n");
        exit(EXIT_FAILURE);
    }

    memoria_compartida->saldo = 0; // Inicializamos el saldo en 0.

    if (sem_init(&memoria_compartida->semaforo, 1, 1) < 0)
    {
        perror("Error al inicializar el semaforo.\n");
        munmap(memoria_compartida, sizeof(MemoriaCompartida));
        exit(EXIT_FAILURE);
    }

    int pipe_credito[2];
    int pipe_debito[2];

    if (pipe(pipe_credito) < 0)
    {
        perror("Error al crear el pipe de Credito.\n");
        sem_destroy(&memoria_compartida->semaforo);
        munmap(memoria_compartida, sizeof(MemoriaCompartida));
        exit(EXIT_FAILURE);
    }

    if (pipe(pipe_debito) < 0)
    {
        perror("Error al crear el pipe de Debito.\n");
        close(pipe_credito[0]);
        close(pipe_credito[1]);
        sem_destroy(&memoria_compartida->semaforo);
        munmap(memoria_compartida, sizeof(MemoriaCompartida));
        exit(EXIT_FAILURE);
    }

    pid_t pid_credito, pid_debito;

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
        credito("credito.txt", pipe_credito);
        exit(EXIT_SUCCESS);
    }

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
        debito("debito.txt", pipe_debito);
        exit(EXIT_SUCCESS);
    }

    printf("[Padre] Esperando transacciones de los hijos...\n");

    // El padre cierra sus extremos de escritura para recibir 0 bytes (EOF) cuando los hijos terminen
    close(pipe_credito[1]);
    close(pipe_debito[1]);

    float monto_leido;

    // Lee el pipe de crédito de forma síncona hasta recibir 0 bytes (hijo finaliza)
    while (read(pipe_credito[0], &monto_leido, sizeof(float)) > 0)
    {
        printf("[Padre] Crédito reporta: +$%.2f\n", monto_leido);
    }

    // Lee el pipe de débito de forma síncona hasta recibir 0 bytes (hijo finaliza)
    while (read(pipe_debito[0], &monto_leido, sizeof(float)) > 0)
    {
        printf("[Padre] Débito reporta:  -$%.2f\n", monto_leido);
    }

    // El padre espera a que ambos procesos dejen el estado zombie
    wait(NULL);
    wait(NULL);

    printf("\n[Padre] El saldo final es: $%.2f\n", memoria_compartida->saldo);

    // LIMPIEZA FINAL DE RECURSOS
    close(pipe_credito[0]);
    close(pipe_debito[0]);
    sem_destroy(&memoria_compartida->semaforo);
    munmap(memoria_compartida, sizeof(MemoriaCompartida));

    printf("[Padre] Recursos liberados y programa finalizado con éxito.\n");

    return 0;
}