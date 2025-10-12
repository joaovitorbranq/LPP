#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

/*
  Versão (a) com 4 processos, sem MPI_Bcast e sem MPI_Get.
  Primitivas usadas: MPI_Init, MPI_Finalize, MPI_Comm_rank, MPI_Comm_size,
                     MPI_Send, MPI_Recv, MPI_Wtime.

  rank 0 (coordenador) -> envia a função, N e o vetor para ranks 1,2,3
  ranks 1,2,3 -> calculam (soma, subtração, multiplicação) e devolvem resultado
*/

enum Role {
    ROLE_NONE = 0,
    ROLE_SUM  = 1,
    ROLE_SUB  = 2,
    ROLE_MUL  = 3
};

enum Tags {
    TAG_ROLE   = 100,  // função atribuída
    TAG_N      = 101,  // tamanho do vetor
    TAG_VET    = 102,  // dados do vetor
    TAG_RESULT = 200   // resultado de volta ao coordenador
};

int main(int argc, char *argv[]) {
    int rank, nprocs;

    MPI_Init(&argc, &argv);                      // inicia o MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);        // id do processo
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);      // total de processos

    const int TAM = 10;
    int N = 10;                 // como no enunciado
    int vet[TAM];

    long long res_sum = 0, res_sub = 0, res_mul = 0;
    double t0 = 0.0, t1 = 0.0;

    if (rank == 0) {
        // Inicializa o vetor
        for (int i = 0; i < N; i++) 
            vet[i] = i + 1;

        // Exibe o vetor
        for (int i = 0; i < N; i++) {
            printf("vet[%d] = %d\n", i, vet[i]);
        }

        t0 = MPI_Wtime(); // início da medição

        // Atribui papéis e envia dados a cada trabalhador
        int roles[3]   = { ROLE_SUM, ROLE_SUB, ROLE_MUL };
        int dest[3] = { 1, 2, 3 };

        for (int i = 0; i < 3; i++) {

            // envia papel
            MPI_Send(&roles[i], 1, MPI_INT, dest[i], TAG_ROLE, MPI_COMM_WORLD);
            // envia N
            MPI_Send(&N, 1, MPI_INT, dest[i], TAG_N, MPI_COMM_WORLD);
            // envia vetor (apenas os N primeiros elementos)
            MPI_Send(vet, N, MPI_INT, dest[i], TAG_VET, MPI_COMM_WORLD);
        }

        // Recebe 3 resultados (um de cada trabalhador)
        for (int i = 0; i < 3; i++) {
            long long tmp = 0;
            MPI_Status st;
            MPI_Recv(&tmp, 1, MPI_LONG_LONG, MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &st);
            if (st.MPI_SOURCE == 1)      res_sum = tmp;
            else if (st.MPI_SOURCE == 2) res_sub = tmp;
            else if (st.MPI_SOURCE == 3) res_mul = tmp;
        }

        t1 = MPI_Wtime(); // fim da medição

        printf("Soma = %lld\n", res_sum);
        printf("Subtracao = %lld\n", res_sub);
        printf("Multiplicacao = %lld\n", res_mul);
        printf("Tempo (segundos): %.6f\n", t1 - t0);
    } else {
        // Trabalhadores: recebem papel, N e vetor
        int my_role = ROLE_NONE;
        int n_local = 0;

        MPI_Recv(&my_role, 1, MPI_INT, 0, TAG_ROLE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&n_local, 1, MPI_INT, 0, TAG_N, MPI_COMM_WORLD, MPI_STATUS_IGNORE);


        int *buffer = (int*)malloc((size_t)n_local * sizeof(int));
        if (!buffer) {
            fprintf(stderr, "Rank %d: falha de alocação\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 3);
        }

        MPI_Recv(buffer, n_local, MPI_INT, 0, TAG_VET, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        long long result = 0;
        if (my_role == ROLE_SUM) {
            long long s = 0;
            for (int i = 0; i < n_local; i++) s += buffer[i];
            result = s;
        } else if (my_role == ROLE_SUB) {
            long long sub = 0;
            for (int i = 0; i < n_local; i++) sub -= buffer[i];
            result = sub;
        } else if (my_role == ROLE_MUL) {
            long long mult = 1;
            for (int i = 0; i < n_local; i++) mult *= buffer[i];
            result = mult;
        }

        free(buffer);
        MPI_Send(&result, 1, MPI_LONG_LONG, 0, TAG_RESULT, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
