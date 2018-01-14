#include <stdio.h>
#include <stdlib.h>

typedef struct list {
	int id;
	struct list *next;
} list;
typedef list *LIST;

void print(LIST test) {
	if (test != NULL) {
		printf("%d\n", test->id);
		print(test->next);
	}
}


int main() {
	LIST A;
	LIST B;
	B->id=2;
	B->next=NULL;
	printf("okB\n");
	print(B);
	
	A->id=1;
	A->next=B;

	printf("created\n");

	print(A);
}
