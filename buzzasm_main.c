#include "buzzasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
   /* Parse command line */
   if(argc != 3) {
      fprintf(stderr, "Usage:\n\t%s <infile.basm> <outfile.bo>\n\n", argv[0]);
      return 0;
   }
   /* Open output file */
   int of = open(argv[2],
                 O_WRONLY | O_CREAT | O_TRUNC,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
   if(of < 0) {
      perror(argv[2]);
   }
   /* Parse script */
   uint8_t* bcode_buf;
   uint32_t bcode_size;
   if(buzz_asm(argv[1], &bcode_buf, &bcode_size) != 0) {
      return 1;
   }
   /* Write to file */
   ssize_t written;
   size_t tot = 0;
   while(tot < bcode_size) {
      written = write(of, bcode_buf + tot, bcode_size - tot);
      if(written < 0) perror(argv[2]);
      tot += written;
   }
   /* Cleanup */
   free(bcode_buf);
   close(of);
   return 0;
}
