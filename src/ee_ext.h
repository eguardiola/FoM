/*****************************************************************************
*
* Ee_Ext.h
*
* Rutinas para leer/grabar en Eeprom Externa 24c64/65
*
****************************************************************************/

#ifndef __EE_EXT_H__
#define __EE_EXT_H__

#include "types.h"
#include <io.h>

#define ADDR_WRITE  0xA0
#define ADDR_READ   0xA1
#define PAGE_SIZE   32

void extee_init(void);
void extee_startCondition(void);
void extee_stopCondition(void);
u08  extee_sendByte(signed char); 
u08  extee_receiveByte(void);

void ee_sendAddr(u16 addr)
{
 extee_startCondition();
 extee_sendByte(ADDR_WRITE);            // write operation
 extee_sendByte((u08)(addr>>8));  // addr high
 extee_sendByte((u08)addr);       // adrr low
}

u08 ee_read(u16 addr)
{
  u08 r;
  
  ee_sendAddr(addr);
  
  extee_startCondition();
  extee_sendByte(ADDR_READ);  //  read mode
  
  r = extee_receiveByte();
  
  extee_stopCondition();
  
  return r;
}

void ee_readString(u08 *dst, u16 addr, u08 len)
{
  ee_sendAddr(addr);
  
 do{
    extee_startCondition();
    extee_sendByte(ADDR_READ);  //  read mode
    *dst++ = extee_receiveByte();
    extee_stopCondition();
   } while(--len);
}


void ee_write(u16 addr,u08 data)
{
 
  if(ee_read(addr) != data)
  {
    ee_sendAddr(addr);
    extee_sendByte(data);  
    extee_stopCondition();
  
    do{
       extee_startCondition();
    }while (extee_sendByte(ADDR_WRITE));  
  
    extee_stopCondition();
  }
}


void ee_writeString(u16 uwAddress, u08 *pubBuffer, u08 ubLen)
{
  u08 ubNumBytes;
  u08 ubPageSize;
  
  if (!ubLen)
    return;
  
  do
  {
    ubPageSize = PAGE_SIZE - (((u08) uwAddress) & (PAGE_SIZE - 1));
    if (ubLen > ubPageSize)
    {
       ubNumBytes = ubPageSize;
       ubLen -= ubPageSize;
    }
    else
    {
       ubNumBytes = ubLen;
       ubLen = 0;    
    }
  
    ee_sendAddr(uwAddress);

    do
    {   
      extee_sendByte(*pubBuffer++);
  } while(--ubNumBytes);
  extee_stopCondition();

  // Esperar el ACK de la EEProm
  do
    {   
      extee_startCondition();
  } while(extee_sendByte(ADDR_WRITE));
  
  // Movernos a la siguiente pagina
  uwAddress += ubPageSize;
} while(ubLen);
}

u08 mem2eeprom_cmp(u16 addr1, u08 *addr2, u08 len)
{
 // compara dos cadenas: una en EEPROM y otra en SRAM
 // devuelve TRUE si son iguales
 u08 cnt;
 
 for (cnt=0; cnt<len; cnt++)
    if (ee_read(addr1+cnt)!=addr2[cnt])
        return 0;

 return 1;
}


// escribe una cadena solo si es diferente a la almacenada
u08 ee_updateString(u16 addr1, u08 *addr2, u08 len)
{
 u08 update_needed = (mem2eeprom_cmp(addr1,addr2,len)==0);
 if (update_needed){
   ee_writeString(addr1,addr2,len);    
 } 
 
 return update_needed;
}




#endif
