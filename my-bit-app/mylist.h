// Noah Hathout
// nhathout
// myList.h

#ifndef MYLIST_H
#define MYLIST_H

#include <stddef.h>

typedef struct Node{
	unsigned int num;
	char binMirror[33];
	char ascii[11];

	struct Node* next;
}Node;

Node* createNode(unsigned int value);

void insertSorted(Node** head, Node* node); //insert binary mirror into position (ASCII lexicographic ascending order)

void printListToFile(FILE* output, const Node* head);

void freeList(Node* head);

void decToBinArr(unsigned int num, char output[33]); //same idea as decToBinary() in bits.c, only bitwise and outputs an array

void decToASCII(unsigned int num, char output[11]); //decimal number into bytewise arra

#endif
