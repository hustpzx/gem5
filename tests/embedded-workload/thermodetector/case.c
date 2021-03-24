/**
 * This is a program which simulates the running of thermodetector, which repr-
 * esents a typical kind of IoT devices.
 * We abstract the load-variation of a thermodetector for epidemic prevention
 * as follow:
 * 00:00 - 07:00    free mode       10%
 * 07:00 - 09:00    morning peak    90%
 * 09:00 - 12:00    normal          50%
 * 12:00 - 13:00    lunch peak      90%
 * 13:00 - 14:30    lunch break     30%
 * 14:30 - 17:00    normal          40%
 * 17:00 - 19:00    leaving peak    70%
 * 19:00 - 22:00    normal          40%
 * 22:00 - 00:00    free mode       10%
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/**
 * uploading involves network I/O, TCP/IP packing, and so on.
 * use basicmath to replace packing and memory IO to replace network IO
 */
int low_load()
{

    char *tmp = (char *)malloc(128 * sizeof(char));
    char c= 'a';
    int i,j;
    for (i=0; i<10; i++)
        for (j=0;j<128;j++){
            tmp[j] = c;
        }

    return 0;
}

int middle_load()
{
    char *tmp = (char *)malloc(512 * sizeof(char));
    char c= 'a';
    int i,j;
    for (i=0; i<10; i++)
        for (j=0;j<512;j++){
            tmp[j] = c;
        }

    return 0;
}

int high_load()
{
    char *tmp = (char *)malloc(1024 * sizeof(char));
    char c= 'a';
    int i,j;
    for (i=0; i<10; i++)
        for (j=0;j<1024;j++){
            tmp[j] = c;
        }

    return 0;
}

int main()
{
    double delay;
    clock_t start;

    // 00:00 - 07:00  10%
    start = clock();
    delay = 7 * 6 * 1000; // 7 hours
    long n=0;
    while (clock() - start < delay)
    {

    }
    printf("(00:00 - 07:00) \n");

    // 07:00 - 08:00,  60%
    start = clock();
    delay = 1 * 6000; // 1 hours
    clock_t last = start;
    double itvl = 200;
    n=0;
    while (clock() - start < delay)
    {
        if (clock() - last >= itvl){
            n++;
            high_load();
            last = clock();
        }
    }
    printf("(07:00 - 08:00),n=%ld \n", n);

    // 08:00 - 09:00, 90%
    start=clock();
    delay = 1 * 6000; //1 hour
    n=0;
    while (clock() - start < delay){
        n++;
        high_load();
    }
    printf("(08:00 - 09:00) n=%ld\n", n);

    // 09:00 - 09:30,  50%
    start = clock();
    delay = 0.5 * 6000; // 1 hours
    last = start;
    itvl = 500;
    n=0;
    while (clock() - start < delay)
    {
        if (clock() - last >= itvl){
            high_load();
            last = clock();
            n++;
        }
    }
    printf("(09:00 - 09:30) n=%ld \n", n);

    // 09:30 - 12:00,  40%
    start = clock();
    delay = 2.5 * 6000; // 2.5 hours
    last = start;
    itvl = 600;
    n=0;
    while (clock() - start < delay)
    {
        if (clock() - last >= itvl){
            n++;
            high_load();
            last = clock();
        }
    }
    printf("(09:30 - 12:00) n=%ld\n", n);

    // 12:00 - 13:00,  90%
    start=clock();
    delay = 1 * 6000; //1 hour
    n=0;
    while (clock() - start < delay){
        n++;
        high_load();
    }
    printf("(12:00 - 13:00) \n");

    // 13:00 - 14:30,  30%
    start = clock();
    delay = 1.5 * 6000; // 1 hours
    last = start;
    itvl = 600;
    n=0;
    while (clock() - start < delay)
    {
        if (clock() - last >= itvl){
            n++;
            high_load();
            last = clock();
        }
    }
    printf("(13:00 - 14:30) n=%ld\n", n);

    // 14:30 - 17:00, low-load 20%
    start = clock();
    delay = 2.5 * 6000; // 2.5 hours
    last = start;
    itvl = 600;
    n=0;
    while (clock() - start < delay)
    {
        if (clock() - last >= itvl){
            high_load();
            last = clock();
            n++;
        }
    }
    printf("(14:30 - 17:00) n=%ld\n", n);

    // 17:00 - 19:00, high load 70%
    start = clock();
    delay = 2 * 6000; // 1 hours
    last = start;
    itvl = 100;
    n=0;
    while (clock() - start < delay)
    {
        if (clock() - last >= itvl){
            high_load();
            last = clock();
            n++;
        }
    }
    printf("(17:00 - 19:00) n=%ld\n", n);

    // 19:00 - 22:00,  40%
    start = clock();
    delay = 3 * 6000; // 3 hours
    last = start;
    itvl = 500;
    n=0;
    while (clock() - start < delay)
    {
        if (clock() - last >= itvl){
            high_load();
            last = clock();
            n++;
        }
    }
    printf("(19:00 - 22:00) n=%ld\n", n);

    // 22:00 - 00:00 low-load 10%
    start = clock();
    delay = 2 * 6000; // 2 hours
    while (clock() - start < delay)
    {

    }
    printf("(22:00 - 00:00) \n");


    return 0;
}


