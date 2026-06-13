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
    sem_t semaforo;     //  sem_t es una variable diseñada específicamente por el sistema operativo para actuar como un semáforo. Si dos procesos intentan modificar este semáforo al mismo exacto milisegundo, Linux se encarga de que no se pisen entre ellos.
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
    if (memoria_compartida == NULL)
    {
        perror("Error al crear la memoria compartida.\n");
        exit(EXIT_FAILURE);
    }
    
    memoria_compartida->saldo = 0;  //  Inicializamos el saldo en 0.

    //  La función sem_init() "enciende" el semáforo y le da sus valores iniciales. Si logra hacerlo bien devuelve un 0, pero si falla por algún motivo, devuelve un -1.
    //  El primer 1 le avisa que este semáforo se va a compartir entre procesos distintos.
    //  El segundo 1 es el valor inicial del semáforo.
    
    if (sem_init(&memoria_compartida->semaforo, 1, 1) < 0)
    {
        perror("Error al inicializar el semaforo.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}