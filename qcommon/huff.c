/* -*-Mode: C;-*- */
/* $Id: huff.c 1.1.1.1 Mon, 17 Jun 1996 18:47:03 -0700 jmacd $	*/
/* huff.c: A library for adaptive huffman encoding and decoding. */

//#include <sys/param.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "huff.h"

#define ERROR_MSG_HEADER "huff: "

#ifndef GNUC
#define inline
#endif

#ifdef DEBUG
#define ASSERT(condition, message) \
if (!(condition)) { \
    fprintf(stderr, "%s:%d: Assertion (%s) failed in %s: %s\n", __FILE__, \
	    __LINE__, #condition, __PRETTY_FUNCTION__, message); \
    abort(); }
#else
#define ASSERT(condition, mes) ((void) 0)
#endif

#ifndef __SVR4
#ifdef sun
int fprintf(FILE*, const char*, ...);
int perror(const char*);
int fgetc(FILE*);
int fputc(int, FILE*);
int fclose(FILE*);
#endif
#endif

static unsigned int Huff_Find_Nth_Zero(HuffStruct* H, int n);
static int Huff_Nth_Zero(HuffStruct* H, int n);
static void Huff_Update_Tree(HuffStruct *H, int n);
static HuffNode* Huff_Increase_Zero_Weight(HuffStruct *H, int n);
static void Huff_Eliminate_Zero(HuffStruct* H, HuffNode *node);
static void Huff_Move_Right(HuffStruct *H, HuffNode *node);
static void Huff_Promote(HuffStruct *H, HuffNode *node);
static void Huff_Init_Node(HuffNode *node, int i, int size);
static Block* Huff_Make_Block(HuffStruct *H, HuffNode *l);
static void Huff_Free_Block(HuffStruct *H, Block *b);
static void Huff_Factor_Remaining(HuffStruct *H);
static inline void Huff_Swap_Ptrs(HuffNode **one, HuffNode **two);
static int Huff_Read_Stats(HuffStruct *H,
			   int AlphabetSize,
			   const char* filename);

/* Huff_Initialize_Adaptive_Encoder()
 *
 * returns an initialized Huffman encoder for an alphabet with the
 * given size.  returns NULL if enough memory cannot be allocated
 */
HuffStruct* Huff_Initialize_Adaptive_Encoder(const int AlphabetSize0)
{
    int TotalNodes;
    int i = 1;
    HuffStruct* ThisEncoder;

    TotalNodes = (2 * AlphabetSize0) - 1;

    ThisEncoder = (HuffStruct*)malloc(sizeof(HuffStruct));
    if(ThisEncoder == NULL) return NULL;
    ThisEncoder->Alphabet   = (HuffNode*)malloc(TotalNodes    * sizeof(HuffNode));
    ThisEncoder->BlockArray = (Block*)   malloc((TotalNodes * 2 )* sizeof(Block));
    ThisEncoder->CodedBits  = (Bit*)     malloc(AlphabetSize0 * sizeof(Bit));
    if(ThisEncoder->CodedBits == NULL ||
       ThisEncoder->Alphabet == NULL  ||
       ThisEncoder->BlockArray == NULL) return NULL;

    ThisEncoder->IsAdaptive = 1;
    ThisEncoder->AlphabetSize = AlphabetSize0;
    ThisEncoder->RootNode = ThisEncoder->Alphabet;
    ThisEncoder->DecodePtr = ThisEncoder->RootNode;
    ThisEncoder->FreeNode = ThisEncoder->Alphabet + AlphabetSize0;
    ThisEncoder->RemainingZeros = ThisEncoder->Alphabet;
    ThisEncoder->CodedDepth = 0;

    ThisEncoder->ZeroFreqCount = AlphabetSize0 + 2;

    Huff_Factor_Remaining(ThisEncoder); /* set ZFE and ZFR */
    Huff_Factor_Remaining(ThisEncoder); /* set ZFDB according to prev state */

    /* ZFC is now AlphabetSize */

    for(i = 0; i < TotalNodes * 2; i += 1) {
	ThisEncoder->BlockArray[i].un.un_freeptr = ThisEncoder->BlockArray + i + 1;
    }

    ThisEncoder->BlockArray[TotalNodes * 2 - 1].un.un_freeptr = NULL;
    ThisEncoder->FreeBlock = ThisEncoder->BlockArray;

    /* Zero frequency nodes are inserted in the first AlphabetSize
     * positions, with Value, Weight, and a pointer to the next zero
     * frequency node.  */
    for(i = ThisEncoder->AlphabetSize - 1; i >= 0; i -= 1) {
	Huff_Init_Node(ThisEncoder->Alphabet + i, i, AlphabetSize0);
    }

    return ThisEncoder;
}

/* Huff_Initialize_Training_Encoder()
 *----------------------------------------------------------------------
 * returns an initialized encoder for encoding via a fixed table and
 * training that table for future uses.
 *----------------------------------------------------------------------
 * ``Huff_Initialize_Training_Encoder'' will create an empty table
 * if parameter table does not exist.  If table exists, it reads the
 * table from that file.  It encodes like the adaptive encoder.
 * @Huff_Dump_Stats (below) will dump two types of table, one is a
 * valid C file which may be included and used as a fixed table for
 * @Huff_Initialize_Fixed_Encoder, the other may be used again by
 * ``Huff_Initialize_Training_Encoder''.
 */
HuffStruct* Huff_Initialize_Training_Encoder(const int AlphabetSize0,
					     const char* filename)
{
    HuffStruct* ThisEncoder = Huff_Initialize_Adaptive_Encoder(AlphabetSize0);

    if(!Huff_Read_Stats(ThisEncoder, AlphabetSize0, filename)) {
	fprintf(stderr, ERROR_MSG_HEADER "Warning: no stats file found, assuming empty\n");
    }

    return ThisEncoder;
}

/* Huff_Initialize_Fixed_Encoder()
 *
 * returns an initialized encoder for encoding via a fixed table which
 * was hard coded.  The header to include can be produced by a stats
 * file using the utility stats2header.
 */
HuffStruct* Huff_Initialize_Fixed_Encoder(const int AlphabetSize0,
					  HuffNode *table)
{
    int TotalNodes;
    int i;
    HuffStruct* ThisEncoder;

    TotalNodes = (2 * AlphabetSize0) - 1;

    ThisEncoder = (HuffStruct*)malloc(sizeof(HuffStruct));
    if(ThisEncoder == NULL) return NULL;
    ThisEncoder->Alphabet   = table;
    ThisEncoder->CodedBits  = (Bit*)malloc(AlphabetSize0 * sizeof(Bit));
    if(ThisEncoder->CodedBits == NULL) return NULL;

    ThisEncoder->IsAdaptive = 0;
    ThisEncoder->AlphabetSize = AlphabetSize0;

    ThisEncoder->RootNode = ThisEncoder->Alphabet + AlphabetSize0;
    ThisEncoder->DecodePtr = ThisEncoder->RootNode;
    ThisEncoder->CodedDepth = 0;

    for(i = 0; i < AlphabetSize0; i += 1)
	table[i].Parent = table + (int)table[i].Parent;

    for(i = AlphabetSize0; i < TotalNodes; i += 1) {
	table[i].Parent = table + (int)table[i].Parent;
	table[i].RightChild = table + (int)table[i].RightChild;
	table[i].LeftChild = table + (int)table[i].LeftChild;
    }

    ThisEncoder->RootNode->Parent = NULL;

    return ThisEncoder;
}

/* Huff_Encode_Data()
 *
 * Takes Huffman transmitter h and n, the nth elt in the alphabet, and
 * returns the number of required to encode n.
 */
int Huff_Encode_Data(HuffStruct* H, int n)
{
    HuffNode *TargetPtr = H->Alphabet + n;

    ASSERT(n < H->AlphabetSize, "Encoded data greater than alphabet size");

    H->CodedDepth = 0;

    /* First encode the binary representation of the nth remaining
     * zero frequency element in reverse such that bit, which will be
     * encoded from H->CodedDepth down to 0 will arrive in increasing
     * order following the tree path.  If there is only one left, it
     * is not neccesary to encode these bits. */
    if(H->IsAdaptive && TargetPtr->Weight == 0) {
	unsigned int where, shift;
	int bits;

	where = Huff_Find_Nth_Zero(H, n);
	shift = 1;
	if(H->ZeroFreqRem == 0)
	    bits = H->ZeroFreqExp;
	else
	    bits = H->ZeroFreqExp + 1;

	while(bits > 0) {
	    if(shift & where)
		H->CodedBits[H->CodedDepth++] = 1;
	    else
		H->CodedBits[H->CodedDepth++] = 0;
	    bits -= 1;
	    shift <<= 1;
	};

	TargetPtr = H->RemainingZeros;
    }

    /* The path from root to node is stacked in reverse so that it is
     * encoded in the right order */
    while(TargetPtr != H->RootNode) {
	if(TargetPtr->Parent->RightChild == TargetPtr)
	    H->CodedBits[H->CodedDepth++] = 1;
	else
	    H->CodedBits[H->CodedDepth++] = 0;

	TargetPtr = TargetPtr->Parent;
    }

    if(H->IsAdaptive)
	Huff_Update_Tree(H, n);

    return H->CodedDepth;
}

/* Huff_Get_Encoded_Bit()
 *
 * Should be called as many times as Huff_Encode_Data returns.
 */
Bit Huff_Get_Encoded_Bit(HuffStruct *H)
{
    ASSERT(H->CodedDepth > 0, "You asked for too many bits");

    H->CodedDepth -= 1;

    return H->CodedBits[H->CodedDepth];
}

/*
 * Huff_Update_Tree --
 *
 *     This procedure updates the tree after Alphabet[n] has been encoded
 *     or decoded.
 */
static void Huff_Update_Tree(HuffStruct *H, int n)
{
    HuffNode *IncrNode;

    if(H->Alphabet[n].Weight == 0) {
	IncrNode = Huff_Increase_Zero_Weight(H, n);
    } else {
	IncrNode = H->Alphabet + n;
    }

    while(IncrNode != H->RootNode) {
	Huff_Move_Right(H, IncrNode);
	Huff_Promote(H, IncrNode);
	IncrNode->Weight += 1;   /* incr the parent */
	IncrNode = IncrNode->Parent; /* repeat */
    }

    H->RootNode->Weight += 1;
}

/*
 * Huff_Move_Right --
 */
static void Huff_Move_Right(HuffStruct *H, HuffNode *MovFwd)
{
    HuffNode **ForParPtr, **BackParPtr;
    HuffNode *MovBack, *tmp;

    MovBack = MovFwd->MyBlock->un.un_leader;

    if(MovFwd == MovBack ||
       MovFwd->Parent == MovBack ||
       MovFwd->Weight == 0)
	return;

    MovBack->RightBlock->LeftBlock = MovFwd;
    if(MovFwd->LeftBlock)
	MovFwd->LeftBlock->RightBlock = MovBack;

    tmp = MovFwd->RightBlock;
    MovFwd->RightBlock = MovBack->RightBlock;
    if(tmp == MovBack)
	MovBack->RightBlock = MovFwd;
    else {
	tmp->LeftBlock = MovBack;
	MovBack->RightBlock = tmp;
    }

    tmp = MovBack->LeftBlock;
    MovBack->LeftBlock = MovFwd->LeftBlock;
    if(tmp == MovFwd)
	MovFwd->LeftBlock = MovBack;
    else {
	tmp->RightBlock = MovFwd;
	MovFwd->LeftBlock = tmp;
    }

    if(MovFwd->Parent->RightChild == MovFwd)
	ForParPtr = &MovFwd->Parent->RightChild;
    else
	ForParPtr = &MovFwd->Parent->LeftChild;

    if(MovBack->Parent->RightChild == MovBack)
	BackParPtr = &MovBack->Parent->RightChild;
    else
	BackParPtr = &MovBack->Parent->LeftChild;

    Huff_Swap_Ptrs(&MovFwd->Parent, &MovBack->Parent);

    Huff_Swap_Ptrs(ForParPtr, BackParPtr);

    MovFwd->MyBlock->un.un_leader = MovFwd;
}

/*
 * Huff_Promote --
 *
 *     Shifts node, the leader of its block, into the next block.
 */
static void Huff_Promote(HuffStruct *H, HuffNode *node)
{
    HuffNode *MyLeft, *MyRight;
    Block *CurBlock;

    MyRight = node->RightBlock;
    MyLeft = node->LeftBlock;
    CurBlock = node->MyBlock;

    if(node->Weight == 0)
	return;

    if(MyLeft == node->RightChild &&
       node->LeftChild &&
       node->LeftChild->Weight == 0) {
	if(node->Weight == MyRight->Weight - 1 && MyRight != H->RootNode) {
	    node->MyBlock = MyRight->MyBlock;
	    MyLeft->MyBlock = MyRight->MyBlock;
	}
	return;
    }

    if(MyLeft != H->RemainingZeros) { /* true if not the leftmost node */
	if(MyLeft->MyBlock == CurBlock)
	    MyLeft->MyBlock->un.un_leader = MyLeft;
	else
	    Huff_Free_Block(H, CurBlock);
    } else {
	return;
    }

    /* node->Parent != MyRight) */
    if((node->Weight == (MyRight->Weight - 1)) && (MyRight != H->RootNode))
	node->MyBlock = MyRight->MyBlock;
    else
	node->MyBlock = Huff_Make_Block(H, node);
}

/*
 * Huff_Increase_Zero_Weight --
 *
 *     When an element is seen the first time this is called to remove it
 *     from the list of zero weight elements and introduce a new internal
 *     node to the tree.
 */
static HuffNode* Huff_Increase_Zero_Weight(HuffStruct *H, int n)
{
    HuffNode *ThisZero, *NewInternal, *ZeroPtr;

    ThisZero = H->Alphabet + n;

    if(H->ZeroFreqCount == 1) {
	/* this is the last one */
	ThisZero->RightChild = NULL;
	if(ThisZero->RightBlock->Weight == 1) {
	    ThisZero->MyBlock = ThisZero->RightBlock->MyBlock;
	} else {
	    ThisZero->MyBlock = Huff_Make_Block(H, ThisZero);
	}
	H->RemainingZeros = NULL;
	return ThisZero;
    }

    NewInternal = H->FreeNode;
    NewInternal->MyBlock = NULL;
    ZeroPtr = H->RemainingZeros;

    H->FreeNode += 1;

    NewInternal->Parent = ZeroPtr->Parent;
    NewInternal->RightBlock = ZeroPtr->RightBlock;
    NewInternal->Weight = 0;
    NewInternal->RightChild = ThisZero;
    NewInternal->LeftBlock = ThisZero;
    NewInternal->MyBlock = Huff_Make_Block(H, NewInternal);

    if(H->RemainingZeros == H->RootNode) {
	/* This is the first element to be coded */
	H->RootNode = NewInternal;
	ThisZero->MyBlock = Huff_Make_Block(H, ThisZero);
    } else {
	NewInternal->RightBlock->LeftBlock = NewInternal;
	if(ZeroPtr->Parent->RightChild == ZeroPtr)
	    ZeroPtr->Parent->RightChild = NewInternal;
	else
	    ZeroPtr->Parent->LeftChild = NewInternal;

	if(NewInternal->RightBlock->Weight == 1) {
	    NewInternal->MyBlock = NewInternal->RightBlock->MyBlock;
	}
	ThisZero->MyBlock = NewInternal->MyBlock;
    }

    Huff_Eliminate_Zero(H, ThisZero);

    NewInternal->LeftChild = H->RemainingZeros;

    ThisZero->RightBlock = NewInternal;
    ThisZero->LeftBlock = H->RemainingZeros;
    ThisZero->Parent = NewInternal;
    ThisZero->LeftChild = NULL;
    ThisZero->RightChild = NULL;

    H->RemainingZeros->Parent = NewInternal;
    H->RemainingZeros->RightBlock = ThisZero;

    return ThisZero;
}

/*
 * Huff_Find_Nth_Zero --
 *
 *     When a zero frequency element is encoded, it is followed by the
 *     binary representation of the index into the remaining elements.
 *     Sets a cache to the element before it so that it can be removed
 *     without calling this procedure again.
 */
static unsigned int Huff_Find_Nth_Zero(HuffStruct* H, int n)
{
    HuffNode *TargetPtr = H->Alphabet + n, *HeadPtr = H->RemainingZeros;
    unsigned int index = 0;

    while(TargetPtr != HeadPtr) {
	HeadPtr = HeadPtr->RightChild;
	index += 1;
    }

    return index;
}

/*
 * Huff_Eliminate_Zero --
 *
 *     Splices node out of the list of zeros.
 */
static void Huff_Eliminate_Zero(HuffStruct* H, HuffNode *node)
{
    if(H->ZeroFreqCount == 1)
	return;

    Huff_Factor_Remaining(H);

    if(node->LeftChild == NULL) {
	H->RemainingZeros = H->RemainingZeros->RightChild;
	H->RemainingZeros->LeftChild = NULL;
    } else if(node->RightChild == NULL) {
	node->LeftChild->RightChild = NULL;
    } else {
	node->RightChild->LeftChild = node->LeftChild;
	node->LeftChild->RightChild = node->RightChild;
    }
}

/*
 * Huff_Init_Node --
 */
static void Huff_Init_Node(HuffNode *node, int i, int Size)
{
    if(i < Size - 1)
	node->RightChild = node + 1;
    else
	node->RightChild = NULL;
    if(i >= 1)
	node->LeftChild = node - 1;
    else
	node->LeftChild = NULL;
    node->Weight = 0;
    node->Parent = NULL;
    node->RightBlock = NULL;
    node->LeftBlock = NULL;
    node->MyBlock = NULL;
}

/*
 * Huff_Swap_Ptrs --
 */
static void Huff_Swap_Ptrs(HuffNode **one, HuffNode **two)
{
    HuffNode *tmpone, *tmptwo;

    tmpone = *one;
    tmptwo = *two;

    *one = tmptwo;
    *two = tmpone;
}

/*
 * Huff_Make_Block --
 *
 *     The data structure used is an array of blocks, which are unions
 *     of free pointers and huffnode pointers.  free blocks are a linked
 *     list of free blocks, the front of which is H->FreeBlock.  The
 *     used blocks are pointers to the head of each block.
 */
static Block* Huff_Make_Block(HuffStruct *H, HuffNode* lead)
{
    Block *ret = H->FreeBlock;

    ASSERT(H->FreeBlock != NULL, "out of blocks");

    H->FreeBlock = H->FreeBlock->un.un_freeptr;

    ret->un.un_leader = lead;

    return ret;
}

/*
 * Huff_Free_Block --
 *
 *     Restores the block to the front of the free list.
 */
static void Huff_Free_Block(HuffStruct *H, Block *b)
{
    b->un.un_freeptr = H->FreeBlock;
    H->FreeBlock = b;
}

/*
 * Huff_Factor_Remaining --
 *
 *     sets ZeroFreqCount, ZeroFreqRem, and ZeroFreqExp to satsity the
 *     equation given above.
 */
static void Huff_Factor_Remaining(HuffStruct *H)
{
    unsigned int i;

    i = (H->ZeroFreqCount -= 1);
    H->ZeroFreqExp = 0;

    while(i > 1) {
	H->ZeroFreqExp += 1;
	i >>= 1;
    }

    i = 1 << H->ZeroFreqExp;

    H->ZeroFreqRem = H->ZeroFreqCount - i;
}

/* Huff_Decode_Bit()
 *
 * receives a bit at a time and returns true when a complete code has
 * been received.
 */
int Huff_Decode_Bit(HuffStruct* H, Bit b)
{
    if(H->IsAdaptive && H->DecodePtr->Weight == 0) {
	int bitsreq;
	if(H->ZeroFreqRem == 0) {
	    bitsreq = H->ZeroFreqExp;
	} else {
	    bitsreq = H->ZeroFreqExp + 1;
	}
	H->CodedBits[H->CodedDepth] = b;
	H->CodedDepth += 1;
	if(H->CodedDepth >= bitsreq) {
	    return 1;
	} else {
	    return 0;
	}
    } else {
	if(b)
	    H->DecodePtr = H->DecodePtr->RightChild;
	else
	    H->DecodePtr = H->DecodePtr->LeftChild;

	if(H->DecodePtr->LeftChild == NULL)
	    return H->DecodePtr->Weight != 0;
	else
	    return 0;
    }
}

/*
 * Huff_Nth_Zero --
 */
static int Huff_Nth_Zero(HuffStruct* H, int n)
{
    HuffNode *Ret = H->RemainingZeros;

    while(n > 0) {
	Ret = Ret->RightChild;
	n -= 1;
    }
    return Ret - H->Alphabet;
}

/* Huff_Decode_Data()
 *
 * once ReceiveBit returns 1, this retrieves an index into the
 * alphabet otherwise this returns 0, indicating more bits are
 * required.
 */
int Huff_Decode_Data(HuffStruct* H)
{
    unsigned int elt = H->DecodePtr - H->Alphabet;

    if(H->IsAdaptive && H->DecodePtr->Weight == 0) {
	int i;
	unsigned int n = 0;
	for(i = 0; i < H->CodedDepth - 1; i += 1) {
	    n |= H->CodedBits[i];
	    n <<= 1;
	}
	n |= H->CodedBits[i];
	elt = Huff_Nth_Zero(H, n);
    }

    H->CodedDepth = 0;

    if(H->IsAdaptive)
	Huff_Update_Tree(H, elt);

    H->DecodePtr = H->RootNode;

    return elt;
}

/* Huff_Delete()
 *
 * deletion.
 */
void Huff_Delete(HuffStruct* H)
{
    free(H->Alphabet);
    free(H->CodedBits);
    free(H->BlockArray);
    free(H);
}

/* Huff_Dump_Stats()
 *
 * write a table.
 */
int Huff_Dump_Stats(HuffStruct* H,
		    const char* filename)
{
    FILE* outfile;
    int i;

    outfile = fopen(filename, "w");

    if(outfile == NULL) {
	perror(ERROR_MSG_HEADER"Fopen failed on stats file");
	return 0;
    }

    fprintf(outfile, "%d ", H->AlphabetSize);

    for(i = 0; i < H->AlphabetSize; i += 1) {
	fprintf(outfile, "%d ", H->Alphabet[i].Weight);
    }

    fclose(outfile);

    return 1;
}

/*
 * Huff_Read_Stats --
 *
 *     Takes a huffstruct and reads the stats file, using the adaptive
 *     routines to build a tree with the correct frequencies.  this is
 *     slower than it could be, but it is assumed that read stats will
 *     only be used in training.
 */
static int Huff_Read_Stats(HuffStruct *H,
			   int AlphabetSize,
			   const char* filename)
{
    FILE* infile = fopen(filename, "r");
    int index = 0;
    int freq;
    int i;

    if(!infile) {
	perror(ERROR_MSG_HEADER"Failed opening stats file");
	return 0;
    }

    if(fscanf(infile, "%d", &freq) != 1) {
	fprintf(stderr, ERROR_MSG_HEADER "Illegal stats file\n");
	return 0;
    }

    if(AlphabetSize != freq) {
	fprintf(stderr, ERROR_MSG_HEADER "Inconsistent alphabet size\n");
	return 0;
    }

    while(index < AlphabetSize) {
	if( fscanf(infile, "%d", &freq) != 1) {
	    fprintf(stderr, ERROR_MSG_HEADER "Illegal stats file\n");
	    return 0;
	}

	for(i = 0; i < freq; i += 1) {
	    Huff_Encode_Data(H, index);
	}

	index += 1;
    }

    fclose(infile);

    return 1;
}

#ifdef DEBUG
int main()
{
    HuffStruct* encoder = Huff_Initialize_Adaptive_Encoder(10);
    int i, j;

    FILE* foo = fopen("/dev/null", "w");

    for(i = 0; i < 10; i += 1) {
	/*for(j = i; j >= 0; j -= 1) { */
	    write_byte(encoder, foo, i);
	    /*} */
    }
}

void print_node(HuffStruct *H, HuffNode *node)
{
    HuffNode *off = H->Alphabet;

    fprintf(stderr, "#<%d (%d) %d> ", /*"#<%snode %d lc %d rc %d w %d l %d> ", */

/* lb %d rb %d p %d  */
/*	    (node == H->RemainingZeros ? "zeros " : ""), */
	    (int)(node - off),
/*	    (node->LeftChild ? (int)(node->LeftChild - off) : -1),
	    (node->RightChild ? (int)(node->RightChild - off) : -1),
	    (node->LeftBlock ? (int)(node->LeftBlock - off) : -1),
	    (node->RightBlock ? (int)(node->RightBlock - off) : -1),
	    (node->Parent ? (int)(node->Parent - off) : -1), */
	    (int)node->Weight,
	    (node->MyBlock ? (int)(node->MyBlock->un.un_leader - off) : -1));
}

int print_depth(HuffStruct *H, HuffNode* node, int d)
{
    if(node == H->RemainingZeros) {
	if(d == 0) {
	    print_node(H, node);
	}
	return 0;
    }

    if(d == 0) {
	print_node(H, node);
	return 1;
    } else {
	int l, r;

	if(node->LeftChild == NULL)
	    l = 0;
	else
	    l = print_depth(H, node->LeftChild, d - 1);

	if(node->RightChild == NULL)
	    r = 0;
	else
	    r = print_depth(H, node->RightChild, d - 1);

	return r + l;
    }
}

void print_tree(HuffStruct *H)
{
    int d = 0;

    while(print_depth(H, H->RootNode, d) > 0)
    {
	fprintf(stderr, "\n");
	d += 1;
    }
}
#endif


/* returns an initialized Adaptive Huffman decoder for an alphabet
 * with the given size.  returns NULL if enough memory cannot be
 * allocated.  */
HuffStruct* Huff_Initialize_Adaptive_Decoder(int AlphabetSize);

/* No iterface is provided for training a set of data while decoding,
 * though this would be easy to implement */

/* returns an initialized Fixed Huffman decoder for an alphabet
 * with the given size. */
HuffStruct* Huff_Initialize_Fixed_Decoder(int AlphabetSize,
					  HuffNode* table);
