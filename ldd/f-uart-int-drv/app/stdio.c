//#include "uart.h"

int putchar(int c)
{
	if (c == '\n')
		uart_putchar('\r');

	if (c == '\b')
	{
		uart_putchar('\b');
		uart_putchar(' ');	
	}

	uart_putchar((char)c);

	return c;	
}

int getchar(void)
{
	int c;

	c = (int)uart_getchar();

	if (c == '\r')
		return '\n';

	return c;
}

int puts(const char * s)
{
	while (*s)
		putchar(*s++);

	// arm-linux-gcc will optimize printf("hello\n") as puts("hello");
	// puts() function should print out '\n' as default
	putchar('\n');

	return 0;
}

char * gets(char * s)
{
	char * p = s;

	// note: here we use getchar(), not uart_getchar() for portability
	while ((*p = getchar()) != '\n')
	{
		if (*p != '\b')
			putchar(*p++);
		else	
			if (p > s)
				putchar(*p--);	
	}

	*p = '\0';
	putchar('\n');

	return s;
}
