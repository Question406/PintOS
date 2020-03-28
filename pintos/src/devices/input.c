#include "devices/input.h"
#include <debug.h>
#include "devices/intq.h"
#include "devices/serial.h"

// #define MYINPUTFUNC

/* Stores keys from the keyboard and serial port. */
static struct intq buffer;

/* Initializes the input buffer. */
void
input_init (void) 
{
  intq_init (&buffer);
}

/* Adds a key to the input buffer.
   Interrupts must be off and the buffer must not be full. */
void
input_putc (uint8_t key) 
{
  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (!intq_full (&buffer));

  intq_putc (&buffer, key);
  serial_notify ();
}

/* Retrieves a key from the input buffer.
   If the buffer is empty, waits for a key to be pressed. */
uint8_t
input_getc (void) 
{

  enum intr_level old_level;
  uint8_t key;

  old_level = intr_disable ();
  key = intq_get  c (&buffer);
  serial_notify ();
  intr_set_level (old_level);
  
  return key;
}

/* Returns true if the input buffer is full,
   false otherwise.
   Interrupts must be off. */
bool
input_full (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);
  return intq_full (&buffer);
}


#ifdef MYINPUTFUNC

int getline(char *str, int lim) {
    char ch;
    int num = 0;
    do {
      ch = input_getc();
      if (ch == 13 || num + 1 == lim) 
        break;
      str[num] = ch;
      ++num;
    } while (true);
    str[num] = 0;
    puts("getline get: ");
    puts(str);
    return num;
}


#endif