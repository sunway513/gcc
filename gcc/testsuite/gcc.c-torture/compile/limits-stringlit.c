/* { dg-skip-if "Array too big" { avr-*-* picochip-*-* m32c-*-* pdp11-*-* msp430-*-* } { "*" } { "" } } */ 

#define STR2 "012345678901234567890123456789012345678901234567890123456789\
0123456789012345678901234567890123456789"
#define STR3 STR2 STR2 STR2 STR2 STR2 STR2 STR2 STR2 STR2 STR2
#define STR4 STR3 STR3 STR3 STR3 STR3 STR3 STR3 STR3 STR3 STR3
#define STR5 STR4 STR4 STR4 STR4 STR4 STR4 STR4 STR4 STR4 STR4
#define STR6 STR5 STR5 STR5 STR5 STR5 STR5 STR5 STR5 STR5 STR5
#define STR7 STR6 STR6 STR6 STR6 STR6 STR6 STR6 STR6 STR6 STR6
#define STR8 STR7 STR7 STR7 STR7 STR7 STR7 STR7 STR7 STR7 STR7

char vlv[] = STR6;
