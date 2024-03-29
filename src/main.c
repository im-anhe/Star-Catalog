// MIT License
// 
// Copyright (c) 2023 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <stdint.h>

//added sys/time and unistd for time
#include <sys/time.h>

//added the thread header
#include <pthread.h>

#include "utility.h"
#include "star.h"
#include "float.h"

#define NUM_STARS 30000 
#define MAX_LINE 1024
#define DELIMITER " \t\n"

struct Star star_array[ NUM_STARS ];
uint8_t   (*distance_calculated)[NUM_STARS];

double  min  = FLT_MAX;
double  max  = FLT_MIN;

//holds the amount of threads
int amt;

//declaring the mutex
pthread_mutex_t mutex;

//when thread is used
double avg_mean = 0;
uint64_t count = 0;

void showHelp()
{
  printf("Use: findAngular [options]\n");
  printf("Where options are:\n");
  printf("-t          Number of threads to use\n");
  printf("-h          Show this help\n");
}

// 
// Embarassingly inefficient, intentionally bad method
// to calculate all entries one another to determine the
// average angular separation between any two stars 


//-t #
void* determineAverageAngularDistance_t( void* p )
{
  int t = (intptr_t) p;

  uint32_t i, j;
  //uint64_t count = 0;

  //bakker calculations :pray:
  int start_t = (NUM_STARS/amt)*t;
  int end_t = ((NUM_STARS/amt)*(t+1)) -1;

  //printf("\n%d / %d", start_t, end_t);


  for (i = start_t; i < end_t; i++)
  {
    for (j = 0; j < NUM_STARS; j++)
    {
      //somewhere within these two for loops - lock and unlock da mutex

      if( i!=j && distance_calculated[i][j] == 0 )
      {          
        double distance = calculateAngularDistance( star_array[i].RightAscension, star_array[i].Declination,
                                                    star_array[j].RightAscension, star_array[j].Declination ) ;
        //don't need to put locks on these next 2
        //threads will access different array elements
        distance_calculated[i][j] = 1;
        distance_calculated[j][i] = 1;

        //locking whatever doesnt use the array
        pthread_mutex_lock(&mutex);
        count++;

        if( min > distance )
        {
          min = distance;
        }

        if( max < distance )
        {
          max = distance;
        }

        avg_mean = avg_mean + (distance-avg_mean)/count;
        //printf("\n%d", avg_mean);
        pthread_mutex_unlock(&mutex);
      }
    }
  }
  return NULL;
}


//unedited function from OG code
//used when they user dont -t #
float determineAverageAngularDistance( struct Star arr[] )
{
  //double mean = 0;
  double mean = 0;

  uint32_t i, j;
  uint64_t count = 0;


  for (i = 0; i < NUM_STARS; i++)
  {
    for (j = 0; j < NUM_STARS; j++)
    {
      if( i!=j && distance_calculated[i][j] == 0 )
      {
        double distance = calculateAngularDistance( arr[i].RightAscension, arr[i].Declination,
                                                      arr[j].RightAscension, arr[j].Declination ) ;
        distance_calculated[i][j] = 1;
        distance_calculated[j][i] = 1;
        count++;

        if( min > distance )
        {
          min = distance;
        }

        if( max < distance )
        {
          max = distance;
        }
        mean = mean + (distance-mean)/count;
      }
    }
  }
  return mean;
}


int main( int argc, char * argv[] )
{

  FILE *fp;
  uint32_t star_count = 0;

  uint32_t n;

  struct timeval begin;
  struct timeval end;

  distance_calculated = malloc(sizeof(uint8_t[NUM_STARS][NUM_STARS]));

  if( distance_calculated == NULL )
  {
    uint64_t num_stars = NUM_STARS;
    uint64_t size = num_stars * num_stars * sizeof(uint8_t);
    printf("Could not allocate %ld bytes\n", size);
    exit( EXIT_FAILURE );
  }

  int i, j;
  // default every thing to 0 so we calculated the distance.
  // This is really inefficient and should be replace by a memset
  for (i = 0; i < NUM_STARS; i++)
  {
    for (j = 0; j < NUM_STARS; j++)
    {
      distance_calculated[i][j] = 0;
    }
  }

  for( n = 1; n < argc; n++ )          
  {
    if( strcmp(argv[n], "-help" ) == 0 )
    {
      showHelp();
      exit(0);
    }
  }

  fp = fopen( "data/tycho-trimmed.csv", "r" );

  if( fp == NULL )
  {
    printf("ERROR: Unable to open the file data/tycho-trimmed.csv\n");
    exit(1);
  }

  char line[MAX_LINE];
  while (fgets(line, 1024, fp))
  {
    uint32_t column = 0;

    char* tok;
    for (tok = strtok(line, " ");
            tok && *tok;
            tok = strtok(NULL, " "))
    {
       switch( column )
       {
          case 0:
              star_array[star_count].ID = atoi(tok);
              break;
       
          case 1:
              star_array[star_count].RightAscension = atof(tok);
              break;
       
          case 2:
              star_array[star_count].Declination = atof(tok);
              break;

          default: 
             printf("ERROR: line %d had more than 3 columns\n", star_count );
             exit(1);
             break;
       }
       column++;
    }
    star_count++;
  }

  printf("%d records read\n", star_count );
  
  //starts the timer
  gettimeofday( &begin, NULL);

  //didnt know what argc and argv to use
  //printf("\n%d \n%s\n", argc, argv[1]);

  //average distance
  double distance;

  //Me: Write the code to create the threads if there is -t
  //printf("\n%d %s\n", argc, argv[1]);
  if(argc == 1)
  {
    //else do it like the given code
    // Find the average angular distance in the most inefficient way possible
    distance =  determineAverageAngularDistance( star_array );
  }
  else if ( (strcmp(argv[1],"-t")==0) && (argv[2] != NULL) )
  {
    amt = atoi(argv[2]);
    //printf("\n%d\n", amt);
    //arr of threads of the chosen amt
    pthread_t arr[amt];
    int count = 1;
    distance = 0;

    //initializes the mutex
    //returns 0 if successful
    if(pthread_mutex_init(&mutex,NULL) != 0)
    {
      perror("Failed to initalize mutex: ");
    }


    //Creating the threads
    //thread_param.c
    for(int i = 0; i< amt; i++)
    {
      if(pthread_create(&arr[i], NULL, &determineAverageAngularDistance_t, (void*)(intptr_t)i)) 
      {
        perror("Error creating thread: ");
        exit( EXIT_FAILURE ); 
      }
    }
        
    //Joining the thread
    //thread_param.c
    for(int i = 0; i<amt; i++)
    {
      if(pthread_join(arr[i], NULL)) 
      {
        perror("Problem with pthread_join: ");
      }
    }

    distance = avg_mean;

  }

  printf("Average distance found is %lf\n", distance );
  printf("Minimum distance found is %lf\n", min );
  printf("Maximum distance found is %lf\n", max );

  //end time
  gettimeofday( &end, NULL);

  float time = ((end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec) / 1e9);
  printf("Program took %f seconds\n", time);

  return 0;
}
