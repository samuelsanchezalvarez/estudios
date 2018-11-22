#define SIZE 64

float a[SIZE][SIZE];
float valx,valy;

int main()
  {
    register int i, j;
    register float x=0.0;
    
    for(j=0; j<SIZE; ++j)
      for(i=0; i<SIZE; ++i)
        x = x + a[i][j];
        
    valx = x;
  }
