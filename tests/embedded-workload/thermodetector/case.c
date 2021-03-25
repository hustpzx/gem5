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

    printf("Smart thermodetector devices simulating begin:\n");
    instTask();

    // 00:00 - 07:00  10%
    start = clock();
    delay = 7 * 6 * 1000; // 7 hours
    long n=0;
    while (clock() - start < delay)
    {

    }
    printf("(00:00 - 07:00) low-load\n");

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
    printf("(07:00 - 08:00),n=%ld high load\n", n);

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
    printf("(09:00 - 09:30) n=%ld middle load\n", n);

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
    printf("(09:30 - 12:00) n=%ld low load\n", n);

    // 12:00 - 13:00,  90%
    start=clock();
    delay = 1 * 6000; //1 hour
    n=0;
    while (clock() - start < delay){
        n++;
        high_load();
    }
    printf("(12:00 - 13:00) high load\n");

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
    printf("(13:00 - 14:30) n=%ld high load\n", n);

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
    printf("(14:30 - 17:00) n=%ld low load\n", n);

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
    printf("(17:00 - 19:00) n=%ld high load\n", n);

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
    printf("(19:00 - 22:00) n=%ld middle load\n", n);

    // 22:00 - 00:00 low-load 10%
    start = clock();
    delay = 2 * 6000; // 2 hours
    while (clock() - start < delay)
    {

    }
    printf("(22:00 - 00:00) low load\n");


    return 0;
}


