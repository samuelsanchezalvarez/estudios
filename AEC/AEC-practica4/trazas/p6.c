#define SIZE 64
float a[SIZE], aa[4], b[SIZE];
float x,y;

int main()
  {
    register int i;
    
    for(i=0; i<SIZE; i++)  /* saltos de 512 bytes */
      x = x * a[i] + b[i];

    for(i=0; i<SIZE; i++)
      y = y * a[i] + b[i];
      
  }
