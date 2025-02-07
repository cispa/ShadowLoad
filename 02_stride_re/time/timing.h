#error "do not include this file directly! rather include time.h of an implementation"

// template for timing implementation.
// The following must be defined in implementing header (either as macro or function):

int time_init(void);
uint64_t timestamp(void);
void time_destroy(void);
