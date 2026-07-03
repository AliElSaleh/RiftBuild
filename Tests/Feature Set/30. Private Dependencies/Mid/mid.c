#include "mid.h"
#include "leaf.h"

#ifndef LEAF_PUBLIC
    #error "Mid consumes Leaf directly, so it must see Leaf's public define"
#endif

int mid_value(void)
{
    return leaf_value() + 10;
}
