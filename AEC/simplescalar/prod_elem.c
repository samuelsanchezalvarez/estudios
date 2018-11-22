#define N 512
float A[N][N], B[N][N], C[N][N];

void main(void) {
  int i,j;
  /* inicializamos las matrices */
  for(i=0;i<N;++i)
    for(j=0;j<N;++j)
      A[i][j] = (float)i/(float)j;
      B[i][j] = (float)j/(float)i;
      C[i][j] = i+j;
      
  /* hacemos el calculo */
  for(i=0;i<N;++i)
    for(j=0;j<N;++j)
      C[i][j] = A[i][j]*B[i][j];

}
