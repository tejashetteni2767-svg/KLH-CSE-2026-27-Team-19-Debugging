#include <stdio.h>

int calculate(int a, int b)
{
    int result = a + b;
    result = result * 2;
    return result;
}

int main(void)
{
    int x = 10;
    int y = 20;

    int result = calculate(x, y);

    printf("Result = %d\n", result);

    return 0;
}
