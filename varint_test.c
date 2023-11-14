#include <stdio.h>
#include <stdlib.h>

int read_varint(char* data)
{
  int numRead = 0;
  int result = 0;
  do {
    int value = (*data & 0b01111111);
    result |= (value << (7 * numRead));

    numRead++;
    if (numRead > 5) {
      printf("VarInt is too big!");
      exit(1);
    }
  } while ((*data++ & 0b10000000) != 0);

  return result;
}

int main(int argc, char* argv[])
{
  char subject1[] = {0};
  char subject2[] = {0b10000000, 0b11000000};
  int res1 = read_varint(subject1);  
  int res2 = read_varint(subject2);  
  printf("Test subject 1: %d\n", res1);
  printf("Test subject 2: %d\n", res2);


  char chained[] = {0, 0};
  printf("Chained test: %d, %d", read_varint(chained), read_varint(chained));

  return 0;
}
