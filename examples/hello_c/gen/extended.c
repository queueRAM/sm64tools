extern void PrintXY(unsigned int x, unsigned int y, const char *str);

const char HelloString[] = "- Hello World -";
unsigned int x = 1;
unsigned int y = 1;
char xdir = 0, ydir = 0;

#define MAX_X 0x100
#define MAX_Y 0xB0

void BehHelloWorld(void)
{
   PrintXY(x, y, HelloString);
   x += xdir ? -1 : 1;
   y += ydir ? -1 : 1;
   if (x == 0 || x == MAX_X) xdir = !xdir;
   if (y == 0 || y == MAX_Y) ydir = !ydir;
}
