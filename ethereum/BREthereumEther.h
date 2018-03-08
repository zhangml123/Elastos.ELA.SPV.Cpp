
#ifndef BR_Ethereum_Ether_H
#define BR_Ethereum_Ether_H

#include "rlp/BRRlpCoder.h"
#include "BRInt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ETHEREUM_BOOLEAN_TRUE = 0,               // INTENTIONALLY 'backwards'
    ETHEREUM_BOOLEAN_FALSE = 1
} BREthereumBoolean;

#define ETHEREUM_BOOLEAN_IS_TRUE(x)  ((x) == ETHEREUM_BOOLEAN_TRUE)
#define ETHEREUM_BOOLEAN_IS_FALSE(x) ((x) == ETHEREUM_BOOLEAN_FALSE)

typedef enum {
    WEI = 0,

    KWEI = 1,
    ADA  = 1,

    MWEI = 2,

    GWEI    = 3,
    SHANNON = 3,

    SZABO = 4,

    FINNEY = 5,

    ETHER = 6,

    KETHER   = 7,
    GRAND    = 7,
    EINSTEIN = 7,

    METHER = 8,
    GETHER = 9,
    TETHER = 10
} BREthereumEtherUnit;

#define NUMBER_OF_ETHER_UNITS  (1 + TETHER)

typedef struct BREthereumEtherStruct {
    BREthereumBoolean positive;
    UInt256 valueInWEI;
} BREthereumEther;

extern BREthereumEther
etherCreate(const UInt256 value, BREthereumEtherUnit unit);

extern BREthereumEther
etherCreateString(const char *string, BREthereumEtherUnit unit);

extern BREthereumEther
etherCreateNumber (uint64_t number, BREthereumEtherUnit unit);

extern BREthereumEther
etherCreateZero (void);

extern UInt256
etherGetValue(const BREthereumEther ether, BREthereumEtherUnit unit);

extern char *
etherGetValueString(const BREthereumEther ether, BREthereumEtherUnit unit);

//extern uint64_t
//etherGetValueNumberDANGEROUS (const BREthereumEther ether, BREthereumEtherUnit unit);

extern BRRlpItem
etherRlpEncode (const BREthereumEther ether, BRRlpCoder coder);

extern BREthereumEther
etherNegate (BREthereumEther);

extern BREthereumEther
etherAdd (BREthereumEther e1, BREthereumEther e2);

extern BREthereumEther
etherSub (BREthereumEther e1, BREthereumEther e2);

//
// Comparisons
//
extern BREthereumBoolean
etherIsEQ (BREthereumEther e1, BREthereumEther e2);

//

extern BREthereumBoolean
etherIsGT (BREthereumEther e1, BREthereumEther e2);

extern BREthereumBoolean
etherIsGE (BREthereumEther e1, BREthereumEther e2);

extern BREthereumBoolean
etherIsZero (BREthereumEther e);

#ifdef __cplusplus
}
#endif

#endif /* BR_Ethereum_Ether_H */
