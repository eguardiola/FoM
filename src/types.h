#ifndef __TYPES_H__
#define __TYPES_H__

typedef unsigned char u08;
typedef unsigned int  u16;

#define kEMM 0
#define kECM 1

// rotar a la derecha (AT90s8515 opt)
u08
rightShift(u08 data, u08 times){
  return data>>times;
}


#endif
