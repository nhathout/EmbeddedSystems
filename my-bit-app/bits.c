// Noah Hathout
// nhathout
// bits.c

#include <stdio.h>
#include <stdlib.h>

// function to convert decimal to binary
// from class 09/10/2025 edited to handle larger numbers (more digits)
unsigned long long decToBinary(unsigned int n)
{
	if (n == 0){
		return 0ULL;
	}

    	unsigned long long scale = 1ULL;
    	unsigned long long n_bin = 0ULL;
	
    	while (n > 0) 
    	{
		n_bin += scale * (n & 1U);
		scale *= 10ULL;
		n = n >> 1;
    	}

    return n_bin;
}

//function to convert binary to decimal
//source: https://www.geeksforgeeks.org/c/c-binary-to-decimal/
unsigned int binaryToDecimal(unsigned int n) {
    unsigned int dec = 0;

    // Initializing base value to 1, i.e 2^0
    unsigned int base = 1;

    // Extracting each digits of binary number
    // and adding corresponding exponent of 2
    while (n) {
        unsigned int last_digit = n % 10;
        n = n / 10;

        // Multiplying the last digit with the base value
        // and adding it to the decimal value
        dec += last_digit * base;

        // Updating the base value by multiplying it by 2
        base = base * 2;
    }

    return dec;
}

//convert int -> binary
//flip binary
//convert back (binary -> int)
//
//The above functions are no longer used in my second attempt but I will keep them to show my thought process
//The above was my first attempt, until I realized C stores all integers as binary (bits) already.
//
//Second attempt I outlined in the Hw1-535.pdf included in the .zip
unsigned int BinaryMirror(unsigned int n)
{
	unsigned int binMirror = 0;

	for(int i = 0; i < 32; ++i) //uses i = i + 1 
	{
		unsigned int lastBit = n & 1;

		binMirror <<= 1;
		if(lastBit == 1){
			binMirror += 1;
		}

		n >>= 1;
	}
	return binMirror;
}

//count '010's (3 state state machine)
unsigned int CountSequence(unsigned int n)
{
	unsigned int count = 0;
	int state = 0;

	for(int i = 31; i >= 0; --i){
		unsigned int bit = (n >> i) & 1; //get most significant bit

		if(state == 0){
			if(bit == 0){
				state = 1;
			}else{
				state = 0;
			}
		}else if(state == 1){
			if(bit == 1){
				state = 2;
			}else{
				state = 1;
			}
		}else if(state == 2){
			if(bit == 0){
				++count;
				state = 1;
			}else{
				state = 0;
			}
		}
	}

	return count;

}
