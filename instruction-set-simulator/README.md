I honestly spent a lot of time just finishing this homework, getting it to compile, 
and running edge cases with different sample assembly codes. For some brief optimization, however,
I was able to find some useful flags for optimizing during compilation.

I first started off by adding the -O3 flag, which I found using this link:https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Option-Summary.html#Option-Summary
I basically looked for optimization flags, found -O2, and then looked up if there were more, which there were. -O3 was the flag that made compilation optimize for speed the most. 

I then also found this reddit thread: https://www.reddit.com/r/C_Programming/comments/wfesjj/what_does_marchnative_do/
The -march=native and -mtune=native flags should tell gcc to prefer output that strongly favors the hosts CPU. So hopefully this will also optimize the code. 

I was also looking into the possibility of lowering the amount of memory used in variables, such as using int8_t instead of int, but I never got around do doing it, and I wasn't sure about how much faster it would make my program.
