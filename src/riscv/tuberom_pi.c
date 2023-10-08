#ifdef __riscv
static int putchar(int c) {
	register int a0 asm ("a0") = c;
	register int a7 asm ("a7") = 0xAC0004;
	asm volatile ("ecall"
                 : "+r" (a0)
                 : "r"  (a7)
                 );
}
#endif

// Print i as 0000.9999
void print4(int i) {
   int p = 1000;
   do {
      char digit = '0';
      while (i >= p) {
         i = i - p;
         digit++;
      }
      p /= 10;
      putchar(digit);
   } while (p);
}

// Calculate ndigits of PI using a spigot algorithm that
// operates in blocks of 4 digits.
//
// Note: ndigits should be a multiple of 4
//
int pi_calc(int ndigits, int *r) {
   int b;
   int c = 0;
   int n = ndigits * 7 / 2;
   for (int i = 1; i <= n; i++)
      r[i] = 2000;
   for (int k = n; k > 0; k -= 14) {
      int d = 0;
      int i = k;
      for(;;) {
         d += r[i] * 10000;
         b = i * 2 - 1;
         r[i] = d % b;
         d /= b;
         i--;
         if (i == 0) break;
         d *= i;
      }
      print4(c + d / 10000);
      c = d % 10000;
   }
}
