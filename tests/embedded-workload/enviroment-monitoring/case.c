/**
 * This is a program which simulates the running of enviroment monitoring devi
 * -ves. The enviroment monitoring devices have specific runing features, peri
 * -odicty, they often collect enviromental data on the hour and stay on low-
 * load status at most of the time.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PI 3.14

typedef struct _envData{
    int temperature;    //degree centigrade
    int humidity;         //percentage
    int wind_direction;   // 8 direction
    int wind_speed;     // unit:m/s
} envData;


/*
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
*/

int getEnvData(envData *data)
{
    /*
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
    */
    // use random function genarating enviromental data
    srand((int)time(NULL));

    data->temperature = rand() % 50 - 10;
    data->humidity = rand() % 100;
    data->wind_direction = rand() % 8;
    data->wind_speed = rand() % 30;

    return 0;
}

/**
 * uploading involves network I/O, TCP/IP packing, and so on.
 * use basicmath to replace packing and memory IO to replace network IO

int uploadData(envData *data)
{
    double  a2 = 1.0, b2 = -4.5, c2 = 17.0, d2 = -30.0;
    double  x[3];
    int     solutions;

    SolveCubic(a2, b2, c2, d2, &solutions, x);

    char *tmp = (char *)malloc(1024 * sizeof(char));
    char c= 'a';
    int i,j;
    for (i=0; i<15; i++)
        for (j=0;j<1024;j++){
            tmp[j] = c;
        }

    int temp = data->temperature;
    int humi = data->humidity;
    int wd = data->wind_direction;
    int ws = data->wind_speed;

    return 0;
}
*/

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

int main()
{
    envData data = {0,0,0,0};
    // delay in clocks, 1000 clocks -> 1 minute
    double lowload_delay = 5000; // 50 minutes at low-load status
    double highload_delay = 500; //10 mintes at high-load
    int h=1;
    long n;
    clock_t start;
    while (h<=24){
        printf("the %dth hour:\n", h);

        start = clock();
        n = 0;
        while (clock() - start < highload_delay)
        {
            n++;
            middle_load();
        }
        getEnvData(&data);
        printf("temperature:%d\thumidity:%d\n",data.temperature,data.humidity);
        printf("wind_direction:%d\twind_speed:%d\nn=%ld\n",
                    data.wind_direction,data.wind_speed,n);

        start = clock();
        //printf("low-load status begin\n");
        while (clock() - start < lowload_delay){
            n++;
        }

        start = clock();
        n=0;
        while (clock() - start < highload_delay){
            middle_load();
            n++;
        }

        printf("period end.\n");

        h++;
    }

    return 0;
}


