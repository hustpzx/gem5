#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MIN(x) (x*1000)

int main()
{
    //double delay = 30; //* CLOCKS_PER_SEC;
    clock_t start = clock();
    //long n=0;
    //printf("start = %ld", start);
    while (clock() - start < 10)
    {
    //    n++;
    }
    //clock_t end = clock();
    //printf("n=%ld , end=%ld\n", n, end);

    return 0;
}
