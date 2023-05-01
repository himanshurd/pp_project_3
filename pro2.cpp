#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#define _USE_MATH_DEFINES
#define CSV

unsigned int seed = 0;

// Global Variables
int	NowYear;		// 2023 - 2028
int	NowMonth;		// 0 - 11

float NowPrecip;		// inches of rain per month
float NowTemp;		// temperature this month
float NowHeight;		// rye grass height in inches
int	NowNumRabbits;		// number of rabbits in the current population

const float RYEGRASS_GROWS_PER_MONTH = 20.0;
const float ONE_RABBITS_EATS_PER_MONTH = 1.0;

const float AVG_PRECIP_PER_MONTH = 12.0;	// average
const float AMP_PRECIP_PER_MONTH = 4.0;	// plus or minus
const float RANDOM_PRECIP = 2.0;	// plus or minus noise

const float AVG_TEMP = 60.0;	// average
const float AMP_TEMP = 20.0;	// plus or minus
const float RANDOM_TEMP = 10.0;	// plus or minus noise

const float MIDTEMP = 60.0;
const float MIDPRECIP =	14.0;

float NowTimothyHeight;   // Timothy grass
const float TIMOTHYGRASS_GROWS_PER_MONTH = 12.0;

float
Ranf( unsigned int *seedp,  float low, float high )
{
    float r = (float) rand_r( seedp );              // 0 - RAND_MAX

    return(   low  +  r * ( high - low ) / (float)RAND_MAX   );
}

float
Sqr( float x )
{
    return x*x;
}

omp_lock_t Lock;
volatile int NumInThreadTeam;
volatile int NumAtBarrier;
volatile int NumGone;

// Barrier function template
void InitBarrier(int n)
{
    NumInThreadTeam = n;
    NumAtBarrier = 0;
    omp_init_lock(&Lock);
}

// Have the calling thread wait here until all the other threads catch up:

void WaitBarrier()
{
    omp_set_lock(&Lock);
    {
        NumAtBarrier++;
        if (NumAtBarrier == NumInThreadTeam)
        {
            NumGone = 0;
            NumAtBarrier = 0;
            // let all other threads get back to what they were doing
            // before this one unlocks, knowing that they might immediately
            // call WaitBarrier( ) again:
            while (NumGone != NumInThreadTeam - 1)
                ;
            omp_unset_lock(&Lock);
            return;
        }
    }
    omp_unset_lock(&Lock);

    while (NumAtBarrier != 0)
        ; // this waits for the nth thread to arrive

#pragma omp atomic
    NumGone++; // this flags how many threads have returned
}

void Rabbits()
{
    while (NowYear < 2029)
    {
        int nextNumRabbits = NowNumRabbits;
        int carryingCapacity = (int) (NowHeight + NowTimothyHeight); 

        if (nextNumRabbits < carryingCapacity)
            nextNumRabbits++;
        else if (nextNumRabbits > carryingCapacity)
            nextNumRabbits--;

        if (nextNumRabbits < 0)
            nextNumRabbits = 0;

        // DoneComputing barrier:
        WaitBarrier();

        NowNumRabbits = nextNumRabbits;

        // DoneAssigning barrier:
        WaitBarrier();

        // DonePrinting barrier:
        WaitBarrier();
    }
}

void RyeGrass()
{
    while (NowYear < 2029)
    {
        float nextHeight = NowHeight;

        float tempFactor = exp(-Sqr((NowTemp - MIDTEMP)/10.));
        float precipFactor = exp(-Sqr((NowPrecip - MIDPRECIP)/10.));

        float totalHeight = NowHeight + NowTimothyHeight; 

        // float percentageEatenByRabbits = totalHeight > 0 ? NowHeight / totalHeight : 0;
       float percentageEatenByRabbits = 0;
            if (totalHeight > 0) {
                percentageEatenByRabbits = NowHeight / totalHeight;
            } else {
                percentageEatenByRabbits = 0;
            }

        nextHeight += tempFactor * precipFactor * TIMOTHYGRASS_GROWS_PER_MONTH;
        nextHeight -= (float) NowNumRabbits * ONE_RABBITS_EATS_PER_MONTH * percentageEatenByRabbits;
        if( nextHeight < 0. ) nextHeight = 0.;

        // DoneComputing barrier:
        WaitBarrier();

        NowHeight = nextHeight;

        // DoneAssigning barrier:
        WaitBarrier();

        // DonePrinting barrier:
        WaitBarrier();
    }
}

void Watcher()
{
    while (NowYear < 2029)
    {
        // DoneComputing barrier:
        WaitBarrier();

        // DoneAssigning barrier:
        WaitBarrier();
        
        #ifdef CSV
                int totalMonths = ((NowYear - 2023) * 12) + NowMonth;
                float temperatureC = (5./9.) * (NowTemp - 32);
                float precipitationCm = NowPrecip * 2.54;
                float heightCm = NowHeight * 2.54;
                float TimothyHeightCm = NowTimothyHeight * 2.54;
                fprintf(stderr, "%4d , %6.2lf , %5.2lf , %6.2lf , %6.2lf , %3d\n",
                        totalMonths, temperatureC, precipitationCm, heightCm, TimothyHeightCm, NowNumRabbits);
        #else
                fprintf(stderr, "Year: %4d ; Month: %2d ; Temp F: %6.2lf ; Precipitation : %5.2lf ; Ryegrass Height : %6.2lf ; Timothygrass Height : %6.2lf ; Rabbits: %3d\n",
                        NowYear, NowMonth, NowTemp, NowPrecip, NowHeight, NowTimothyHeight, NowNumRabbits);
        #endif

        NowMonth++;
        if (NowMonth > 12) {
            NowMonth = 1;
            NowYear++;
        }

        float ang = (  30.*(float)NowMonth + 15.  ) * ( M_PI / 180. );
        float temp = AVG_TEMP - AMP_TEMP * cos( ang );
        NowTemp = temp + Ranf( &seed, -RANDOM_TEMP, RANDOM_TEMP );
        float precip = AVG_PRECIP_PER_MONTH + AMP_PRECIP_PER_MONTH * sin( ang );
        NowPrecip = precip + Ranf( &seed,  -RANDOM_PRECIP, RANDOM_PRECIP );

        if( NowPrecip < 0. )
            NowPrecip = 0.;

        // DonePrinting barrier:
        WaitBarrier();
    }
}

void MyAgent()
{
    while (NowYear < 2029)
    {
        float nextTimothyHeight = NowTimothyHeight;

        float tempFactor = exp(-Sqr((NowTemp - MIDTEMP + 20.)/10.));
        float precipFactor = exp(-Sqr(( NowPrecip - MIDPRECIP)/10.));

        float totalHeight = NowHeight + NowTimothyHeight;
        float percentageEatenByRabbits = totalHeight > 0 ? NowTimothyHeight / totalHeight : 0;
        nextTimothyHeight += tempFactor * precipFactor * TIMOTHYGRASS_GROWS_PER_MONTH;
        nextTimothyHeight -= (float) NowNumRabbits * ONE_RABBITS_EATS_PER_MONTH * percentageEatenByRabbits;

        if( nextTimothyHeight < 0. ) nextTimothyHeight = 0.;

        // DoneComputing barrier:
        WaitBarrier();

        NowTimothyHeight = nextTimothyHeight;

        // DoneAssigning barrier:
        WaitBarrier();

        // DonePrinting barrier:
        WaitBarrier();
    }
}

void run_simulation()
{
    omp_set_num_threads(4);

    #pragma omp parallel sections
    {
        #pragma omp section
        {
            Rabbits();
        }

        #pragma omp section
        {
            RyeGrass();
        }

        #pragma omp section
        {
            Watcher();
        }

        #pragma omp section
        {
            MyAgent();  // your own
        }
    }
}

int main( int argc, char *argv[ ] )
{
    #ifdef _OPENMP
        //fprintf( stderr, "OpenMP is supported -- version = %d\n", _OPENMP );
    #else
        fprintf( stderr, "No OpenMP support!\n" );
        exit(1);
    #endif

    #ifdef CSV
        fprintf( stderr, "Month,TempC,Precipitation cm,RyegrassHeight cm,TimothygrassHeight cm,Num Rabbits\n");
    #endif
 
    // starting date and time:
    NowMonth = 0;
    NowYear  = 2023;
            
    // starting state (feel free to change this if you want):
    NowNumRabbits = 1;
    NowHeight =  5.;

    omp_set_num_threads(4);	
    InitBarrier(4);	

    float ang = (  30.*(float)NowMonth + 15.  ) * ( M_PI / 180. );
    float temp = AVG_TEMP - AMP_TEMP * cos( ang );
    NowTemp = temp + Ranf( &seed, -RANDOM_TEMP, RANDOM_TEMP );
    float precip = AVG_PRECIP_PER_MONTH + AMP_PRECIP_PER_MONTH * sin( ang );
    NowPrecip = precip + Ranf( &seed,  -RANDOM_PRECIP, RANDOM_PRECIP );

    if( NowPrecip < 0. )
        NowPrecip = 0.;
        
    omp_set_num_threads(4);
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            Rabbits();
        }

        #pragma omp section
        {
            RyeGrass();
        }

        #pragma omp section
        {
            Watcher();
        }

        #pragma omp section
        {
            MyAgent();  // your own
        }
}
}