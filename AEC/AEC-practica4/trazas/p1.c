#define SIZE 64
float a[SIZE], b[SIZE], c[SIZE];
float valx,valy;

int main()
  {
    register int i;
    register float x=0.0;
    register float y=0.0;
    
    for(i=0; i<SIZE; ++i)
      x = x * a[i] + b[i];

    for(i=0; i<SIZE; ++i)
      y = y * a[i] + c[i];
      
    valx = x;
    valy = y;
  }
