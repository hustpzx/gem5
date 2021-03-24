/**
 * This is a program which simulates the running of smart-home devices(SHD).
 * We abstract the load-variation of typical SHDs as follow:
 * 00:00 - 07:00    Sleeping & No-disturbing mode(low-load 10%)
 * 07:00 - 09:00    alarm clock, morning-news & weather broadcasting (middle-
 *                  load, involving decode&encode of voice, network IO)
 * 09:00 - 12:30    on-time alarm, noon-news broadcasting (low-load 20%)
 * 12:30 - 14:30    lunch breaking(No-disturbing mode), alarm-clock(low-load)
 * 14:30 - 17:30    on-time alarm(low-load 20%)
 * 17:30 - 22:00    Active interaction with SHDs(high-load 80%)
 * 22:00 - 00:00    Sleeping & No-disturbing mode(low-load 10%)
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PI 3.14

void SolveCubic(double  a,
                double  b,
                double  c,
                double  d,
                int    *solutions,
                double *x)
{
      long double    a1 = b/a, a2 = c/a, a3 = d/a;
      long double    Q = (a1*a1 - 3.0*a2)/9.0;
      long double R = (2.0*a1*a1*a1 - 9.0*a1*a2 + 27.0*a3)/54.0;
      double    R2_Q3 = R*R - Q*Q*Q;

      double    theta;

      if (R2_Q3 <= 0)
      {
            *solutions = 3;
            theta = acos(R/sqrt(Q*Q*Q));
            x[0] = -2.0*sqrt(Q)*cos(theta/3.0) - a1/3.0;
            x[1] = -2.0*sqrt(Q)*cos((theta+2.0*PI)/3.0) - a1/3.0;
            x[2] = -2.0*sqrt(Q)*cos((theta+4.0*PI)/3.0) - a1/3.0;
      }
      else
      {
            *solutions = 1;
            x[0] = pow(sqrt(R2_Q3)+fabs(R), 1/3.0);
            x[0] += Q/x[0];
            x[0] *= (R < 0.0) ? 1 : -1;
            x[0] -= a1/3.0;
      }
}


int instTask()
{

    // use SolveCubic() to replace ADC
    double  a1 = 1.0, b1 = -10.5, c1 = 32.0, d1 = -30.0;
    double  a2 = 1.0, b2 = -4.5, c2 = 17.0, d2 = -30.0;
    double  a3 = 1.0, b3 = -3.5, c3 = 22.0, d3 = -31.0;
    double  a4 = 1.0, b4 = -13.7, c4 = 1.0, d4 = -35.0;
    double  x[3];
    int  solutions;

    SolveCubic(a1, b1, c1, d1, &solutions, x);
    SolveCubic(a2, b2, c2, d2, &solutions, x);
    SolveCubic(a3, b3, c3, d3, &solutions, x);
    SolveCubic(a4, b4, c4, d4, &solutions, x);

    return 0;
}

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

    instTask();
    //printf("Smart-Home device simulating begin:\n");

    // 00:00 - 07:00 low-load 10%
    start = clock();
    delay = 7 * 6 * 1000; // 7 hours
    long n=0;
    while (clock() - start < delay)
    {

    }
    printf("(00:00 - 07:00) n=%ld\n",n);

    // 07:00 - 09:00, middle-load 40%
    start = clock();
    delay = 1.5 * 6000; // 1.5hours
    clock_t last = start;
    double itvl = 500;
    n=0;
    while (clock() - start < delay)
    {
        if (clock() - last < itvl){
            n++;
        } else{
            high_load();
            last = clock();
        }
    }
    start = clock();
    while (clock() -start < 3000 ){
        n++;
        n = n+5;
        n = n-5;
    }
    printf("(07:00 - 09:00) n=%ld\n",n);

    // 09:00 - 12:00, low-load 20%
    int i;
    n=0;
    for (i=0;i<3;i++){
        low_load(); // on-time alarm
        start = clock();
        delay = 6000; // 1 hour
        while (clock() - start < delay)
        {
            n++;
            n = n + 5;
        }
    }
    printf("(09:00 - 12:00) \n");

    // 12:00 - 13:00, middle-load 30%
    start = clock();
    delay = 6000; // 1 hour
    last = start;
    itvl = 600;
    n=0;
    while (clock() - start < delay)
    {
        if (clock() - last < itvl){
            n++;
        } else{
            middle_load();
            last = clock();
        }
    }
    printf("(12:00 - 13:00) n=%ld\n", n);

    // 13:00 - 14:00, low-load 1-%
    start=clock();
    delay = 6000;
    n=0;
    while (clock() - start < delay){

    }
    printf("(13:00 - 14:00) \n");

    // 14:00 - 17:00, low-load 20%
    n=0;
    for (i=0;i<3;i++){
        low_load(); // on-time alarm
        start = clock();
        delay = 6000; // 1 hour
        while (clock() - start < delay)
        {
            n++;
            n = n + 5;
        }
    }
    printf("(14:00 - 17:00) \n");

    // 17:00 - 22:00, high load 80%
    start=clock();
    delay = 5 * 6000; //5hour
    n=0;
    while (clock() - start < delay){
        n++;
        high_load();
    }
    printf("(17:00 - 22:00) n=%ld\n", n);


    // 22:00 - 00:00 low-load 10%
    start = clock();
    delay = 2 * 6000; // 2 hours
    while (clock() - start < delay)
    {

    }
    printf("(22:00 - 00:00) \n");


    return 0;
}


