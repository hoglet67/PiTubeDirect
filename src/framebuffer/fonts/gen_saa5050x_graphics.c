#include <stdio.h>

void main() {

   printf("#ifndef SAA505X_GRAPHICS_FNT_H\n");
   printf("#define SAA505X_GRAPHICS_FNT_H\n\n");

   for (int sep = 0; sep < 2; sep++) {

      printf("// %s Graphics\n\n", sep ? "Separated" : "Contiguous");

      int l = sep ? 0x18 : 0x38;
      int r = sep ? 0x03 : 0x07;

      for (int c = 0; c < 64; c++) {

         int b00 = c & 1;
         int b01 = c & 2;
         int b10 = c & 4;
         int b11 = c & 8;
         int b20 = c & 16;
         int b21 = c & 32;

         printf("   ");

         for (int row = 0; row < 10; row++) {
            int data = 0;
            if (row < 3) {
               if (b00) {
                  data |= l;
               }
               if (b01) {
                  data |= r;
               }
            } else if (row < 7) {
               if (b10) {
                  data |= l;
               }
               if (b11) {
                  data |= r;
               }
            } else {
               if (b20) {
                  data |= l;
               }
               if (b21) {
                  data |= r;
               }
            }
            if (sep && (row == 2 || row == 6 || row == 9)) {
               data = 0;
            }
            printf("0x%02x, ", data);
         }
         printf("\n");
      }
      printf("\n");
   }

   printf("#endif\n");

}
