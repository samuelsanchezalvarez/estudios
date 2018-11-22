int a[32768];
int temp;

int multiplica_ent (int a, int b) {
register int t, resultado;

  resultado=0;
  for (t=0; t<b; t++)
    resultado += a;
  return resultado;
}

void main() {
  register int i;
  register int t=0;

  for(i=0; i<32768; i+=32) {
    a[i]=10;  
    t += multiplica_ent(t,a[i]);
  }
  temp=t;
}
