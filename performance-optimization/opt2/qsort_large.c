#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define UNLIMIT
#define MAXARRAY 60000 /* this number, if too large, will cause a seg. fault!! */

struct my3DVertexStruct {
  int x, y, z;
  double distance;
};

int compare(const void *elem1, const void *elem2)
{
  /* D = [(x1 - x2)^2 + (y1 - y2)^2 + (z1 - z2)^2]^(1/2) */
  /* sort based on distances from the origin... */

  double distance1, distance2;

  distance1 = (*((struct my3DVertexStruct *)elem1)).distance;
  distance2 = (*((struct my3DVertexStruct *)elem2)).distance;

  return (distance1 > distance2) ? 1 : ((distance1 == distance2) ? 0 : -1);
}


int
main(int argc, char *argv[]) {
  struct my3DVertexStruct array[MAXARRAY];
  FILE *fp;
  int i,j,count=0;
  int x, y, z;
  
  if (argc<2) {
    fprintf(stderr,"Usage: qsort_large <file>\n");
    exit(-1);
  }
  else {
    fp = fopen(argv[1],"r");
    
    while((fscanf(fp, "%d", &x) == 1) && (fscanf(fp, "%d", &y) == 1) && (fscanf(fp, "%d", &z) == 1) &&  (count < MAXARRAY)) {
	 array[count].x = x;
	 array[count].y = y;
	 array[count].z = z;

	 //nhathout I noticed use of pow() & sqrt()
	 //
	 //Instead of calculating distance this way, square distance 
	 //should still work (only used to compare)
	 
	 //x, y, and z are ints so cast to double just in case
	 array[count].distance = (double)x*(double)x + (double)y*(double)y + (double)z*(double)z;
	 count++;
    }
  }
  printf("\nSorting %d vectors based on distance from the origin.\n\n",count);
  
  //nhathout wrapping qsort function in a loop (100 times); as Piazza said for eng-grid tests
  //not for submission
  qsort(array,count,sizeof(struct my3DVertexStruct),compare);
  
  for(i=0;i<count;i++)
    printf("%d %d %d\n", array[i].x, array[i].y, array[i].z);
  return 0;
}
