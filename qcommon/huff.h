/* -*-Mode: C;-*- */
/* $Id: huff.h 1.1.1.1 Mon, 17 Jun 1996 18:47:03 -0700 jmacd $	*/
/* huff.h: A library for adaptive huffman encoding and decoding. */

#ifndef _HUFF_H_
#define _HUFF_H_

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

/*
 * This is used as the frequency counter for nodes in the Huffman
 * tree.  If one thought the possibility of overflowing this counter
 * was likely, it could be coded into the library to reset the
 * frequency table after transmitting this many codes.
 */
typedef unsigned int u_int32_t;
typedef u_int32_t weight_t;

typedef struct HuffStruct HuffStruct;
typedef struct HuffNode HuffNode;
typedef unsigned char Bit;

typedef struct Block {
    union {
	struct HuffNode* un_leader;
	struct Block* un_freeptr;
    } un;
} Block;

/*
 * struct HuffNode --
 *
 *     Weight is a count of the number of times this element has been
 *     seen in the current encoding/decoding.  Parent, RightChild, and
 *     LeftChild are pointers defining the tree structure.  RightBlock
 *     and LeftBlock point to neighbors in an ordered sequence of
 *     weights.  The left child of a node is always guaranteed to have
 *     weight not greater than its sibling.  BlockLeader points to the
 *     element with the same weight as itself which is closest to the
 *     next increasing weight block.  */
struct HuffNode {
    weight_t Weight;
    struct HuffNode *Parent;
    struct HuffNode *LeftChild;
    struct HuffNode *RightChild;
    struct HuffNode *LeftBlock;
    struct HuffNode *RightBlock;
    Block *MyBlock;
};

/*
 * struct HuffStruct --
 *
 *
 *     AlphabetSize is the a count of the number of possible leaves in
 *     the huffman tree.  The number of total nodes counting internal
 *     nodes is ((2 * AlphabetSize) - 1).  ZeroFreqCount is the number
 *     of elements remaining which have zero frequency.  ZeroFreqExp
 *     and ZeroFreqRem satisfy the equation ZeroFreqCount =
 *     2^ZeroFreqExp + ZeroFreqRem.  RootNode is the root of the tree,
 *     which is initialized to a node with zero frequency and contains
 *     the 0th such element.  FreeNode contains a pointer to the next
 *     available HuffNode space.  Alphabet contains all the elements
 *     and is indexed by N.  RemainingZeros points to the head of the
 *     list of zeros.  */
struct HuffStruct {
    int AlphabetSize;
    int ZeroFreqCount;
    int ZeroFreqExp;
    int ZeroFreqRem;
    int CodedDepth;

    int IsAdaptive;

    Bit *CodedBits;

    Block *BlockArray;
    Block *FreeBlock;

    HuffNode *DecodePtr;
    HuffNode *RemainingZeros;
    HuffNode *Alphabet;
    HuffNode *RootNode;
    HuffNode *FreeNode;

    const char* Tag;
};

/*                             Encoder                               */

/* returns an initialized Huffman encoder for an alphabet with the
 * given size.  returns NULL if enough memory cannot be allocated.  */
EXTERN HuffStruct* Huff_Initialize_Adaptive_Encoder(const int AlphabetSize);

/* returns an initialized encoder for encoding via a fixed table which
 * was hard coded.  The header to include can be produced by a stats
 * file using the utility stats2header.  */
EXTERN HuffStruct* Huff_Initialize_Fixed_Encoder(const int AlphabetSize,
						 HuffNode* table);

/* returns an initialized encoder for encoding via a fixed table and
 * training that table for future uses.  It will create an empty table
 * if parameter table does not exist.  If table exists, it reads the
 * table from that file.  It encodes like the adaptive encoder.
 * Huff_Dump_Stats (below) will dump two types of table, one is a
 * valid C file which may be included and used as a fixed table for
 * Huff_Initialize_Fixed_Encoder, the other may be used again by
 * Huff_Initialize_Training_Encoder.  */
EXTERN HuffStruct* Huff_Initialize_Training_Encoder(const int AlphabetSize,
						    const char* statsfile);

/* Takes Huffman transmitter h and n, the nth elt in the alphabet, and
 * returns the number of required to encode n.  */
EXTERN int Huff_Encode_Data(HuffStruct* h, int n);

/* should be called as many times as TransmitData returns.  */
EXTERN Bit Huff_Get_Encoded_Bit(HuffStruct* h);

/* 			       Decoder                               */

/* Encoder and decoder structs are identical, call the initialization
 * function for the correct type encoder and begin calling the following
 * two procedures to decode data. */

/* receives a bit at a time and returns true when a complete code has
 * been received.  */
EXTERN int Huff_Decode_Bit(HuffStruct* h, Bit b);

/* once ReceiveBit returns 1, this retrieves an index into the
 * alphabet otherwise this returns 0, indicating more bits are
 * required.  */
EXTERN int Huff_Decode_Data(HuffStruct* h);

/* deletion */
EXTERN void Huff_Delete(HuffStruct* h);

/* write a table */
EXTERN int Huff_Dump_Stats(HuffStruct* H, const char* statsfile);

#endif
