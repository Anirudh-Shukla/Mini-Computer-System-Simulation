/* Lab 5 entry point: starts the operating-system shell. */
#include <stdio.h>
#include "os.h"
int main(void)
{
    printf("========================================\n");
    printf("       MINI COMPUTER SIMULATOR\n");
    printf("========================================\n");
    os_start();
    printf("\n========================================\n");
    printf("       SIMULATOR TERMINATED\n");
    printf("========================================\n");
    return 0;
}
