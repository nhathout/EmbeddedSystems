// Noah Hathout
// nhathout
// main.c

#include <stdio.h>
#include <stdlib.h>
#include "bits.h"
#include "mylist.h"

int main(int argc, char* argv[])
{
	if(argc != 3){
		fprintf(stderr, "Usage: %s <input.txt> <output.txt>\n", argv[0]); //error checking for incorrect run command
		return 1;
	}

	const char* input_path = argv[1];
	const char* output_path = argv[2];

	FILE *input;
	input = fopen(input_path, "r"); //read
       	if(!input){
		perror("Error opening input file");
		return 1;
	}

	FILE *output;
	output = fopen(output_path, "w"); //overwrites/cleans
	if(!output){
		perror("Error opening output file");
		return 1;
	}

	unsigned int num;
	while(fscanf(input, "%u", &num) == 1){ //while unsigned int exists (also is stored in num)
		unsigned int binMirror = BinaryMirror(num);
		unsigned int count = CountSequence(num);

		fprintf(output, "%u\t%u\n", binMirror, count); //print to output file with tab in the middle
	}

	fprintf(output, "\n");
	
	//to rewind the input file, credit: https://www.geeksforgeeks.org/c/rewind-in-c/
	rewind(input);

	Node* head = NULL;
	unsigned int num2;
	while(fscanf(input, "%u", &num2) == 1){
		Node *node = createNode(num2);
		insertSorted(&head, node);
	}

	printListToFile(output, head);
	freeList(head);
	
	fclose(input);
	fclose(output);
	return 0;	
}
