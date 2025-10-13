#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

/*
    Versão (b) — modelo mestre/escravo com qualquer número de processos.
    Utiliza apenas: MPI_Init, MPI_Comm_rank, MPI_Comm_size, MPI_Send, MPI_Recv,
    MPI_Wtime, MPI_Isend, MPI_Irecv.

    Cada processo calcula a soma e a multiplicação de uma parte do vetor.
    O mestre (rank 0) consolida os resultados.
*/

static void divide_vetor(int N, int P, int *counts, int *displs) {
    // divide N elementos em P blocos
    // arg *counts = quantos elementos cada processo recebe
    // arg *displs = índice inicial de cada processo no vetor
    int base = N / P;
    int resto = N % P;
    int desloc = 0;
    for (int i = 0; i < P; i++) {
        counts[i] = base + (i < resto ? 1 : 0);
        displs[i] = desloc;
        desloc += counts[i];
    }
}

int main(int argc, char *argv[]) {
    int rank, nprocs;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    int N = 10;
    int vet[10];
    double t0 = 0.0, t1 = 0.0;

    // inicializacoes
    if (rank == 0) {
        for (int i = 0; i < N; i++) 
            vet[i] = i + 1;
        printf("Vetor:\n");
        for (int i = 0; i < N; i++) 
            printf("vet[%d] = %d\n", i, vet[i]);
        t0 = MPI_Wtime();
    }

    // distribui tarefas
    int *counts = (int*)malloc(nprocs * sizeof(int));
    int *displs = (int*)malloc(nprocs * sizeof(int));
    divide_vetor(N, nprocs, counts, displs);

    int local_n = counts[rank];
    int *local_buf = (int*)malloc(local_n * sizeof(int));

    // mestre envia tamanho e pedaço do vetor
    if (rank == 0) {
        for (int r = 1; r < nprocs; r++) {
            MPI_Send(&counts[r], 1, MPI_INT, r, 100, MPI_COMM_WORLD);
            if (counts[r] > 0) {
                MPI_Request req;
                MPI_Isend(&vet[displs[r]], counts[r], MPI_INT, r, 101, MPI_COMM_WORLD, &req);
                MPI_Wait(&req, MPI_STATUS_IGNORE);
            }
        }
        // copia para o buffer local do mestre a sua própria parte do vetor global.
        memcpy(local_buf, &vet[displs[0]], local_n * sizeof(int));
    } else {
        // slaves recebem tamanho e o pedaco do vetor
        MPI_Recv(&local_n, 1, MPI_INT, 0, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (local_n > 0) {
            local_buf = realloc(local_buf, local_n * sizeof(int));
            MPI_Request req;
            MPI_Irecv(local_buf, local_n, MPI_INT, 0, 101, MPI_COMM_WORLD, &req);
            MPI_Wait(&req, MPI_STATUS_IGNORE);
        }
    }

    // cálculos locais para cada processo
    int local_sum = 0;
    int local_mul = 1;
    for (int i = 0; i < local_n; i++) {
        local_sum += local_buf[i];
        local_mul *= local_buf[i];
    }

    // envio
    int results[2] = { local_sum, local_mul };
    if (rank == 0) {
        // mestre recebe de todos
        int (*all_results)[2] = malloc(nprocs * sizeof *all_results);
        MPI_Request *reqs = malloc(nprocs * sizeof(MPI_Request));

        for (int r = 0; r < nprocs; r++)
            MPI_Irecv(all_results[r], 2, MPI_INT, r, 200, MPI_COMM_WORLD, &reqs[r]);

        // mestre envia para si mesmo (poderia copiar direto, mas é só para exemplificar Isend/Irecv)
        MPI_Request self_req;
        MPI_Isend(results, 2, MPI_INT, 0, 200, MPI_COMM_WORLD, &self_req);

        // MPI_Waitall eh o equivalente a for (...) {MPI_Wait}
        MPI_Waitall(nprocs, reqs, MPI_STATUSES_IGNORE);

        // Consolida resultados
        int global_sum = 0;
        int global_mul = 1;
        for (int r = 0; r < nprocs; r++) {
            global_sum += all_results[r][0];
            global_mul *= all_results[r][1];
        }
        int global_sub = -global_sum;

        t1 = MPI_Wtime();

        printf("\nResultados finais:\n");
        printf("Soma = %d\n", global_sum);
        printf("Subtracao = %d\n", global_sub);
        printf("Multiplicacao = %d\n", global_mul);
        printf("Tempo (segundos): %.6f\n", t1 - t0);

        free(all_results);
        free(reqs);
    } else {
        // slaves enviam resultados com Isend
        MPI_Request req;
        MPI_Isend(results, 2, MPI_INT, 0, 200, MPI_COMM_WORLD, &req);
        MPI_Wait(&req, MPI_STATUS_IGNORE);
    }

    free(local_buf);
    free(counts);
    free(displs);
    MPI_Finalize();
    return 0;
}



