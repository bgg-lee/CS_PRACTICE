#include <stdio.h>
#include <stdlib.h>

typedef struct nodeType LinkedNode;

struct nodeType{
    int val;
    LinkedNode *next; //typedef 선언
};

LinkedNode *createNode(int x){ //node를 가르키는 pointer반환,python에서 __init__에 해당하던 내용,여기까지가 Node 만드는 과정
    LinkedNode *newNode;
    newNode = (LinkedNode*)malloc(sizeof(LinkedNode)); //malloc 중요!
    newNode->val = x;
    newNode->next = NULL;
    return newNode;
}

typedef struct listType SLList;

struct listType{
    LinkedNode *first;
    int size;
};



void addFirst(SLList*LL,int x);
int getFirst(SLList*LL);
int getSize(SLList*LL);
void printSLList(SLList*LL);

//Implementatino of Deletion
LinkedNode* searchNode(SLList*LL,int x);
void deleteNode(SLList*LL,int x);

int main(void){

    SLList myLL = {NULL, 0}; // 이런식으로 initialize 하는구만
    deleteNode(&myLL,999);
    printSLList(&myLL); //1
    addFirst(&myLL,10);
    printSLList(&myLL); //2
    addFirst(&myLL,11);

    printSLList(&myLL); //3
    addFirst(&myLL,15);
    addFirst(&myLL,7);   

    printSLList(&myLL); //4
    deleteNode(&myLL,100);
    printSLList(&myLL); //5

    deleteNode(&myLL,11);
    printSLList(&myLL); //6

    addFirst(&myLL,99);   
    deleteNode(&myLL,7);
    printSLList(&myLL); //7


    return 0;
}

void addFirst(SLList*LL,int x){
    LinkedNode *newFirst = createNode(x);
    newFirst->next = LL->first;
    LL->first = newFirst;
    LL->size ++;
}

int getFirst(SLList*LL){
    if(LL->first == NULL) return -1;
    return LL->first->val;
}

int getSize(SLList*LL){
    return LL->size;
}

void printSLList(SLList*LL){
    LinkedNode *temp = LL->first;
        printf("size:%d, first:%d, allvals: ",getSize(LL),getFirst(LL));
    while(temp){
        printf("%d->",temp->val);
        temp = temp->next;
    }
    printf("END\n");
}


LinkedNode* searchNode(SLList*LL,int x){
    
    LinkedNode* temp = LL->first;
    while(temp){
        if(temp->val == x){
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}
void deleteNode(SLList*LL,int x){ // 사실 Search 안 써도 여기 안에 한 번에 구현 가능
    LinkedNode *temp = LL->first;
    LinkedNode *prev = NULL; // 얘가 신의 한수
    
    while(temp){
        
        if(temp == searchNode(LL,x)){
            prev->next = temp->next;
            temp->next = NULL;
            free(temp);
            LL->size--;
            return;
        }
        prev = temp;
        temp = temp->next;
    }
    return;
}