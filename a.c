#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

/*
  Versão MPI com 4 processos:
  - rank 0: coordenador
  - rank 1: soma
  - rank 2: subtração
  - rank 3: multiplicação

  PRIMITIVAS MPI UTILIZADAS (comentadas ao longo do código):
    - MPI_Init / MPI_Finalize: inicia/finaliza o ambiente MPI.
    - MPI_Comm_rank / MPI_Comm_size: obtém o rank (id) do processo e o número de processos.
    - MPI_Bcast: difusão (broadcast) de dados do root (rank 0) para todos os processos.
    - MPI_Send / MPI_Recv: envio/recebimento ponto-a-ponto de mensagens (para distribuir função e coletar resultados).
    - MPI_Wtime: mede tempo de execução (relógio de parede).
*/

enum Role {
    ROLE_NONE = 0,
    ROLE_SUM  = 1,
    ROLE_SUB  = 2,
    ROLE_MUL  = 3
};

enum Tags {
    TAG_ROLE     = 100,  // tag para envio da função
    TAG_RESULT   = 200   // tag para envio dos resultados
};

int main(int argc, char *argv[]) {
    int rank, nprocs;

    // 1) Inicializa o MPI e descobre rank (id) e total de processos
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);   // id deste processo
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs); // total de processos

    // Precisamos exatamente de 4 processos conforme enunciado
    if (nprocs != 4) {
        if (rank == 0) {
            fprintf(stderr, "Este programa requer exatamente 4 processos (mpirun -np 4).\n");
        }
        MPI_Finalize();
        return 1;
    }

    // Parâmetros do problema (mantendo o exemplo do enunciado)
    const int TAM = 1000; // capacidade do vetor (não precisa ser totalmente utilizada)
    int vet[TAM];
    int N = 10;           // vamos usar apenas 10 elementos como no exemplo

    // Variáveis para resultados (usaremos tipos maiores para multiplicação para evitar overflow)
    long long res_sum = 0;     // soma cabe em long long com folga
    long long res_sub = 0;     // subtração idem
    long long res_mul = 1;     // multiplicação cresce rápido; long long ajuda a evitar overflow

    double t0 = 0.0, t1 = 0.0;

    // 2) O coordenador (rank 0) inicializa o vetor e envia as funções
    if (rank == 0) {
        // inicialização do vetor: vet[x] = x + 1, para x = 0..N-1
        for (int x = 0; x < N; x++) vet[x] = x + 1;

        // mede o tempo a partir daqui
        t0 = MPI_Wtime();

        // ENVIA a função para cada trabalhador (via MPI_Send ponto-a-ponto)
        // rank 1 -> soma, rank 2 -> subtração, rank 3 -> multiplicação
        int role_sum = ROLE_SUM;
        int role_sub = ROLE_SUB;
        int role_mul = ROLE_MUL;
        MPI_Send(&role_sum, 1, MPI_INT, 1, TAG_ROLE, MPI_COMM_WORLD);
        MPI_Send(&role_sub, 1, MPI_INT, 2, TAG_ROLE, MPI_COMM_WORLD);
        MPI_Send(&role_mul, 1, MPI_INT, 3, TAG_ROLE, MPI_COMM_WORLD);
    }

    // 3) Broadcast dos metadados e do vetor:
    //    - O coordenador (root=0) "difunde" N e os N elementos de vet para TODOS os processos.
    //    - Assim garantimos que todos têm os mesmos dados de entrada.
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);     // tamanho efetivo do vetor
    // Observação: o conteúdo válido está nos primeiros N elementos.
    MPI_Bcast(vet, N, MPI_INT, 0, MPI_COMM_WORLD);    // difunde o vetor

    // 4) Trabalhadores: recebem a sua função do coordenador
    if (rank != 0) {
        int my_role = ROLE_NONE;
        MPI_Recv(&my_role, 1, MPI_INT, 0, TAG_ROLE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Executa a função designada
        if (my_role == ROLE_SUM) {
            long long s = 0;
            for (int i = 0; i < N; i++) s += vet[i];
            // envia resultado de volta ao coordenador
            MPI_Send(&s, 1, MPI_LONG_LONG, 0, TAG_RESULT, MPI_COMM_WORLD);
        } else if (my_role == ROLE_SUB) {
            // subtração acumulada: partindo de 0 como no enunciado
            long long sub = 0;
            for (int i = 0; i < N; i++) sub -= vet[i];
            MPI_Send(&sub, 1, MPI_LONG_LONG, 0, TAG_RESULT, MPI_COMM_WORLD);
        } else if (my_role == ROLE_MUL) {
            // multiplicação acumulada: partindo de 1 como no enunciado
            long long mul = 1;
            for (int i = 0; i < N; i++) mul *= vet[i];
            MPI_Send(&mul, 1, MPI_LONG_LONG, 0, TAG_RESULT, MPI_COMM_WORLD);
        }
        // trabalhadores encerram
        MPI_Finalize();
        return 0;
    }

    // 5) Coordenador (rank 0): imprime o vetor, recebe e imprime resultados
    if (rank == 0) {
        // Apenas para aderir ao exemplo, mostramos o vetor:
        for (int x = 0; x < N; x++) {
            printf("vet[%d] = %d\n", x, vet[x]);
        }

        // Recebe 3 resultados (um de cada trabalhador).
        // Podemos identificar pelo número da fonte (source) no status, se quisermos.
        for (int i = 0; i < 3; i++) {
            long long tmp;
            MPI_Status st;
            MPI_Recv(&tmp, 1, MPI_LONG_LONG, MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &st);

            if (st.MPI_SOURCE == 1) {         // veio do processo da soma
                res_sum = tmp;
            } else if (st.MPI_SOURCE == 2) {  // veio do processo da subtração
                res_sub = tmp;
            } else if (st.MPI_SOURCE == 3) {  // veio do processo da multiplicação
                res_mul = tmp;
            }
        }

        t1 = MPI_Wtime();

        // Imprime os resultados (observação: multiplicação pode crescer MUITO rápido!)
        printf("Soma = %lld\n", res_sum);
        printf("Subtracao = %lld\n", res_sub);
        printf("Multiplicacao = %lld\n", res_mul);
        printf("Tempo (segundos): %.6f\n", t1 - t0);
    }

    MPI_Finalize();
    return 0;
}
