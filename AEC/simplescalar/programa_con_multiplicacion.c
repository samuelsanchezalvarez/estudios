int a[32768];
int temp;

void main() {
  register int i;
  register int t=0;

  for(i=0; i<32768; i+=32) {
    a[i]=10;  
    t += t*a[i];
  }
  temp=t;
}
