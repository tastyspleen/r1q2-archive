/* MD4 context. */

typedef ptrdiff_t * POINTER;

typedef struct
{
	uint32	state[4];		/* state (ABCD) */
	uint32	count[2];		/* number of bits, modulo 2^64 (lsb first) */
	byte	buffer[64]; 	/* input buffer */
} MD4_CTX;

void MD4_Init (MD4_CTX *);
void MD4_Update (MD4_CTX *, byte *, uint32);
void MD4_Final (byte[16], MD4_CTX *);
