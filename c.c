#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

/*
  Pipeline MPI (np >= 2), N <= 10.
  Primitivas: MPI_Init, MPI_Comm_rank, MPI_Comm_size, MPI_Send, MPI_Recv, MPI_Wtime, MPI_Wtick.

  Mensagem (5 ints):
    msg[0] = value    (elemento atual)
    msg[1] = sum_acc  (soma acumulada)
    msg[2] = sub_acc  (subtração acumulada: 0 - soma)
    msg[3] = mul_acc  (multiplicação acumulada)
    msg[4] = end_flag (1 = sentinela/fim)
*/

enum { TAG_DATA = 0, TAG_DONE = 99 };

typedef struct {
    int do_sum;
    int do_sub;
    int do_mul;
} Ops;

/* Atribui operações por estágio (ranks 1..p-1) */
static Ops atribui_ops(int rank, int nprocs) {
    Ops ops = (Ops){0,0,0};
    if (rank == 0) return ops;     // fonte não opera

    int workers = nprocs - 1;      // estágios de trabalho
    int idx = rank - 1;            // 0-based

    if (workers == 1) {
        ops.do_sum = ops.do_sub = ops.do_mul = 1;
    } else if (workers == 2) {
        if (idx == 0) ops.do_sum = 1;
        else          ops.do_sub = ops.do_mul = 1;
    } else {
        if      (idx == 0) ops.do_sum = 1;
        else if (idx == 1) ops.do_sub = 1;
        else if (idx == 2) ops.do_mul = 1;
        // idx >= 3 → só repassa
    }
    return ops;
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    const int N = 10;     // até 10 elementos
    int vet[N];
    double t0 = 0.0, t1 = 0.0;

    int prev = rank - 1;
    int next = rank + 1;

    Ops ops = atribui_ops(rank, nprocs);

    if (rank == 0) {
        // Fonte: gera dados
        for (int i = 0; i < N; i++) vet[i] = i + 1;

        // Mostra resolução do timer
        double res = MPI_Wtick();
        printf("MPI_Wtick (resolucao do timer): %.6f s\n", res);

        // Início da medição
        t0 = MPI_Wtime();

        // Envia elementos
        for (int i = 0; i < N; i++) {
            int msg[5] = { vet[i], 0, 0, 1, 0 };
            MPI_Send(msg, 5, MPI_INT, next, TAG_DATA, MPI_COMM_WORLD);
        }
        // Envia sentinela
        int endmsg[5] = { 0, 0, 0, 1, 1 };
        MPI_Send(endmsg, 5, MPI_INT, next, TAG_DATA, MPI_COMM_WORLD);

        // Espera ACK do último rank (sumidouro)
        int ack = 0;
        MPI_Recv(&ack, 1, MPI_INT, nprocs - 1, TAG_DONE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Fim da medição (mesmo relógio do rank 0)
        t1 = MPI_Wtime();
        printf("Tempo (segundos): %.9f\n", t1 - t0);
    }
    else if (rank < nprocs - 1) {
        // Estágio intermediário com acumuladores locais
        int sum_acc = 0, sub_acc = 0, mul_acc = 1;

        while (1) {
            int msg[5];
            MPI_Recv(msg, 5, MPI_INT, prev, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (msg[4] == 1) {
                // Fim: repassa sentinela e encerra
                MPI_Send(msg, 5, MPI_INT, next, TAG_DATA, MPI_COMM_WORLD);
                break;
            }

            int value = msg[0];
            if (ops.do_sum) sum_acc += value, msg[1] = sum_acc;
            if (ops.do_sub) sub_acc -= value, msg[2] = sub_acc;
            if (ops.do_mul) mul_acc *= value, msg[3] = mul_acc;

            MPI_Send(msg, 5, MPI_INT, next, TAG_DATA, MPI_COMM_WORLD);
        }
    }
    else {
        // Último estágio (sumidouro)
        int sum_acc = 0, sub_acc = 0, mul_acc = 1;

        while (1) {
            int msg[5];
            MPI_Recv(msg, 5, MPI_INT, prev, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (msg[4] == 1) {
                // Imprime resultados finais
                printf("Soma = %d\n", sum_acc);
                printf("Subtracao = %d\n", sub_acc);
                printf("Multiplicacao = %d\n", mul_acc);

                // Envia ACK ao mestre para fechamento da medição
                int ack = 1;
                MPI_Send(&ack, 1, MPI_INT, 0, TAG_DONE, MPI_COMM_WORLD);
                break;
            }

            int value = msg[0];

            // Se este estágio faz alguma operação, acumula
            if (ops.do_sum) sum_acc += value;
            if (ops.do_sub) sub_acc -= value;
            if (ops.do_mul) mul_acc *= value;

            // Caso operações venham de estágios anteriores:
            if (!ops.do_sum && msg[1] != 0) sum_acc = msg[1];
            if (!ops.do_sub && msg[2] != 0) sub_acc = msg[2];
            if (!ops.do_mul && msg[3] != 1) mul_acc = msg[3];
        }
    }

    MPI_Finalize();
    return 0;
}
