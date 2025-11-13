// Noah Hathout
// nhathout
// myList.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylist.h"
#include "bits.h"

void decToBinArr(unsigned int num, char output[33])
{
	for(int i = 31; i >= 0; --i){
		output[31-i] = ((num >> i) & 1) ? '1' : '0'; //same concept as decToBinary function in bits.c but void and working with an array
	}

	output[32] = '\0'; //null terminator
}

//convert to decimal string which can be used in strcmp()
void decToASCII(unsigned int num, char output[11])
{
	if(num == 0){ //edge-case (num is 0 so just fill output with '0' and null terminator
		output[0] = '0';
		output[1] = '\0';
	}else{
		char temp[11];
		int count = 0;

		while(num > 0){
			unsigned int currBit = num % 10;
			temp[count] = (char)('0' + currBit);
			count++;
			num /= 10;
		}

		//reverse into output[] to match input num
		for(int i = 0; i < count; ++i){
			output[i] = temp[count - 1 - i];
		}

		output[count] = '\0';
	}
}

Node* createNode(unsigned int x)
{
	Node* node = (Node*)malloc(sizeof(Node));
	if(!node){ //error allocating memory for node
		return NULL;
	}

	node->num = x;

	unsigned int mirr = BinaryMirror(x);
	
	decToBinArr(mirr, node->binMirror);
	decToASCII(mirr, node->ascii);

	return node;
}

//function to take in two node pointers and strcmp their ASCII
//strcmp() documentation states that "characters in the same pos from both strings are
//compared one by one starting from the left. returns 0 if equal, >0 if first > second,
// <0 if first < second
//
// source: https://www.w3schools.com/c/ref_string_strcmp.php
static int cmpNodes(const Node* a, const Node* b)
{
	int cmp = strcmp(a->ascii, b->ascii);
	
	//realizing this function wasn't necessary because it's basically just one line
	return cmp;
}


void insertSorted(Node** head, Node* node)
{
	if(!head || !node) //no nodes were made
	{
		return;
	}

	if(*head == NULL || cmpNodes(node, *head) < 0) //head is greater lexicographically than insert
	{
		//make insert head
		node->next = *head;
		*head = node;
		return;
	}

	Node *curr = *head;
	while(curr->next && cmpNodes(node, curr->next) >= 0) //while there is a next node and the insert is bigger, move insert down
	{
		curr = curr->next;
	}

	node->next = curr->next; 
	curr->next = node;
}

void printListToFile(FILE* output, const Node* head)
{
	for(const Node *curr = head; curr; curr = curr->next)
	{
		fprintf(output, "%s\n", curr->ascii);
	}
}

void freeList(Node* head)
{
	while(head)
	{
		Node* next = head->next;
		free(head);
		head = next;
	}
}
