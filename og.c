#include <stdio.h>
int main ()
{
int x, soma=0, subtracao=0, mult=1, TAM = 1000;
int vet[TAM];
for (x=0;x<10;x++) {
vet[x] = x + 1;
}
for (x=0;x<10;x++) {
printf("vet[%d] = %d\n", x, vet[x]);
}
for (x=0;x<10;x++) {
soma = soma + vet[x];
subtracao = subtracao - vet[x];
mult = mult * vet[x];
}
printf("Soma = %d\n", soma);
printf("Subtracao = %d\n", subtracao);
printf("Multiplicacao = %d\n", mult);
}