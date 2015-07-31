/*********************************************************************
    Fichero:    FoM V1.03  (FunSECA)
    Autor:      Fun-o-Matic
    Fecha:      Nov2000 - Julio2001
                20010810. Renacimiento
    Proposito:  Emular a una SECA SmartCard para fines didácticos.
    Compilador: AVR-GCC + Make2 de gcctest.zip
    Hardware:   FunCard y Apollo de funcard.net
**********************************************************************/

#include <io.h>
#include "types.h"
#include "funseca.h"
#include "funio.h"
#include "decrip.h"
#include "ee_ext.h"
#include "ee_int.h"

#define CLA 0
#define INS 1
#define P1  2
#define P2  3
#define LEN 4


// Definen los modos de funcionamiento
#define AUTOPPUA
#define MODO_LOG
#define AUTOPBM
#define SERIAL

#define MAX_COMMAND_LENGHT 0x5A  // (+5 de la cabecera = 0x5f)
#define EVENT_ID1              2
#define EVENT_ID2              3
#define SESSION_ID             4
#define SESSION_DATE1          9
#define SESSION_DATE2         10

// Pin  7799 -> Cambiar el número de proveedores (sólo uno o real).
// Pin  7788 -> ModoMultiEPG.
#define PIN_NPRO_H   0x1E
#define PIN_NPRO_L   0x77
#define PIN_MEPG_L   0x6C

/*------------------------------------------------------------------------
|
| El modo SERIAL utiliza la UART del 8515 para transmitir el tráfico
| entre la tarjeta y el deco.
| Para poder ver el tráfico en el PC, hace falta soldar dos cables a la
| tarjeta y un circuito adaptador de niveles TTL->RS232. Este circuito
| puede ser perfectamente una tarjeta Season. Los cables que hay que
| soldar son TX y masa. Según el tipo de integrado (DIP de 40 pines o
| SMD de 64 pines) tenemos:
|
|       SMD    DIP
| TX    pin7   pin11
| masa  pin16  pin20
|
| En funcard con leds, TX se puede sacar del cátodo del diodo 6 (con
| letra v minúscula). El cátodo es el pin donde no va la resistencia.
|
| diodos V V v V  V N R
|
| La parte difícil del invento, salvado lo anterior, es averiguar la
| frecuencia de transmisión que es función del reloj con que trabaja
| la tarjeta que, a su vez, varía según los modelos de decodificadores.
| Los valores dados son para un deco oficial Philips
+-------------------------------------------------------------------------*/
#ifdef SERIAL
  #define	CLOCK       3579545		// clock frequency
  #define	BAUDRATE    38400      // choose a baudrate

  // 22 --> 10470
  // 15 --> 15600
  // 12 --> 19200

  #define	UBRR_VALUE  12 //(CLOCK/(16*BAUDRATE))-1
  #define UDRE        5
  #define TX_SERIAL(Valor) {SerialSend(&Valor,1);}
  u08 modePhoenix = 1;
#endif

#define NUM_PPV_EVENTS 15*3
u08 PPVRecords[NUM_PPV_EVENTS];
u08 posPPVRecs;

u08 funomatic[16-2]={'F','u','n','-','o','-','m','a','t','i','C',' ','v','1'};

// Simular registro de compra
u08 ppv_record[14]={0xB1,0x00,0xFF,0xFF,0xFF,0xFE,0x05,0x00,0x00,0xFF,0xFF,0x00,0x00,0x04};
// Simular credit record
u08 ppv_credit_record[10]={0x84,0x00,0x00,0x00,0x00,0x00,0x17,0x3E,0x00,0x04};
// Punteros a las funciones de comunicación
void (*ptrSend)(u08);
u08  (*ptrReceive) (void);


// info mode (On Screen Display):
// - 0: show provider + expiration date (mode info off)
// - 1: show Channel id, operation Key, etc (mode info on)
u08 infoMode;


// currentProvider, currentKey & currentChannel for info mode
u08 currentProvider;
u08 currentKey;
u08 currentChannel;


// Auto-actualizar el PBM.
// Si el Channel Boundle ID permanece cte en valor y posición
// durante tres ECM seguidas se autoactualiza
// Solo el que este en primera posición.
#ifdef AUTOPBM
u08 autoPBM;
u08 repeat_bid;
u08 last_bid = 0xff;
#endif

u08 decodeX;    // Para el bloqueo de taquillas
u08 multiEPG;   // multi EPG mode
u08 decoNoOfi;  // Comprobar en nano 31 de la 3C
u08 lockPBM;
u08 needPin    = TRUE;
u08 pinChecked; // = FALSE

// Answer to Reset
// Indica que la tarjeta es una versión 4.0
u08 ATR[16]={0x3B,0xF7,0x11,0x00,0x01,0x40,0x96,0x54,0x30,0x04,0x0E,0x6C,0xB6,0xD6, 0x90, 0x00};

// Datos para la Inst 0x0A: Sistema Operativo de la tarjeta
#define TOTAL_VERSION 68
u08 version[TOTAL_VERSION]= {
  0x61,0x19,0xad,0xca,0x15,0x2f,0x10,0x03,
  0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
  0xff,0xff,'(' ,'c' ,')' ,'F' ,'u' ,'n' ,
  '-' ,'o' ,'-' ,'m' ,'a' ,'t' ,'i' ,'C' ,
  ' ' ,'2' ,'0' ,'0' ,'1' ,' ' ,'V' ,'1' ,
  '.' ,'0' ,'4' ,0x00 };

// Buffer de entrada y salida
u08 buffer[MAX_COMMAND_LENGHT];

// clave en uso (PK + SK)
u08 Key[8+8];

// hash-buffers para la comprobación de la firma
u08 hash[8];
u08 initial_hash[8];

// ControlWords desencriptadas para la 0x3A
u08 CW[8+8];

// comando enviado por la 0x34
u08 Comando34[3];

u08 auxDate[2];

// status bytes
u08 sb1,sb2;

void sendString(u08 *str, u08 len)
{
  do { (*ptrSend)(*str++); } while (--len);
}

void receiveString(u08 *dst, u08 len)
{
//  u08 *dst=buffer;

  do { *dst++ = (*ptrReceive)(); } while (--len);
}

void fillBuffer(u08 begin_idx ,u08 end_idx, u08 fillByte)
{
  u08 cnt;

  for (cnt=begin_idx; cnt<end_idx; cnt++)
    buffer[cnt]=fillByte;
}

void _memcpy(u08 *dst,u08 *src,u08 n)
{
 while(n){
   *dst=*src;
   dst++;
   src++;
   n--;
  }
}


u16 getProvAddr(u08 p1)
{
  u16 res;

  p1 &= 0x0f;
  for (res = EE_ENTETY_0; p1; p1--)
    res += EE_ENTETY_SIZE;

  return res;
}


u16  getPKAddr(u08 p1, u08 p2)
{
  u16 key_offset = getProvAddr(p1) + EE_ENT_KEY_0;

  //key_index*=8, at908515 opt
  p2 <<= 1; p2 <<= 1; p2 <<= 1;

  return (key_offset + p2);
}


void  decode(u08 p2, u08 len)
{
  u08 *src=buffer;

  if (p2&0x80){
    while (len>8){
      decrip(Key,src);
      len-=8;
      src+=8;
    }
  }
}

void getKey(u08 p1, u08 p2)
{
  u16 key_offset=getPKAddr(p1,p2);

  ee_readString(Key,key_offset,8);
  ee_readString(Key+8,(p1&0x10)?key_offset+(16*8):key_offset,8);
}


u08 keyExist(u08 p1, u08 p2)
{
  // 0x00 si la clave no existe
  // 0x50 si sólo PK
  // 0xf0 si tambien SK (nota: la PK puede ser 8*0x00)

  u08 cnt,res=0x00;

  // Cargar Pk&Sk
  getKey(p1,p2);

  for (cnt=0; cnt<16; cnt++)
  {
    if (Key[cnt])
    {
      if (cnt<8)
      {
        res=0x50;
      }
      else
      {
        res=0xf0;
      }
   }
 }
  return res;
}

u08 xor_hash(u08 len, u08 r)
{
  u08 cnt;

  decrip(Key,hash);
  len-=r;
  for (cnt=0; cnt<r; cnt++)
    hash[cnt]^=buffer[len+cnt];

  return len;
}

u08 signatureCheck(u08 p1, u08 len)
{
  u08 cnt;
  u08 resto=len&7;
  u16 addr;


  len-=8; // indice al nano 0x82
  if (buffer[len-1]!=0x82)
    return 0;

  _memcpy(hash,buffer+len,8);

  // calculo inverso de la firma

  if (resto){
    len=xor_hash(len,resto);
  }

  while (len>0){
    len=xor_hash(len,8);
  }

  // Calcular el hashbuffer inicial
  for (cnt=0; cnt<8; cnt++){
    initial_hash[cnt]=0;
  }

  addr=getProvAddr(p1) + EE_ENT_PPUA;
  p1&=0xE0;
  if (p1==0x20){
    // UA + 00 00
    ee_readString(initial_hash, EE_NUM_TARJETA,6);
  } else
  if (p1==0x40){
    // PPUA + 00 00 00 00
    ee_readString(initial_hash,addr,4);
  } else
  if (p1==0x60){
    // SA + 00 00 00 00 00
    ee_readString(initial_hash,addr,3);
  }

  // Si firma correcta -> hash buffer calculado = hash buffer inicial
  for (cnt=0; cnt<8; cnt++){
    if (initial_hash[cnt]!=hash[cnt])
      return 0;
  }

  return 1;
}


u08 updateKey(u08 idx, u08 p1, u08 sk)
{
  u16 addr;

  // desencriptar la clave
  _memcpy(CW,&buffer[idx+2],8);
  decrip(Key,CW);

  // calcular dirección
  addr=getPKAddr(p1,(buffer[idx+1]&0x0F));
  addr=sk?(addr+(16*8)):addr;

  // actualizar si es diferente a la almacenada
  return(ee_updateString(addr,CW,8));
}


u08 nibble2ascii(u08 nibble)
{
  return (nibble<0x0a)?(nibble+'0'):(nibble+('A'-0x0a));
}

void byte2ascii(u08 value, u08 idx)
{
  u08 *dst=&buffer[idx];

  *dst++=nibble2ascii(value>>4);
  *dst=nibble2ascii(value&0x0f);
}

void setupInfo(u08 p1, u08 p3)
{
  u16 addr;
  u08 i,j,n;

  if (infoMode){
    // Rellenar con espacios campo nombre
    fillBuffer(2,(2+16)-1,0x20);
    addr=getProvAddr(currentProvider);
    // PPUA + Fecha
    ee_readString(buffer+(2+16),addr+EE_ENT_PPUA,4+2);

    p1&=0x0f;
    if (p1==0x01){
      _memcpy(buffer+(2+1),funomatic,16-2);
    } else
    if (p1==0x02){
      ee_readString(buffer + 2, addr + EE_ENT_NAME, 16);
    } else
    if (p1==0x03){
      if (currentChannel){
        // "P:xxxx/X  K:xx  "
        ee_readString(buffer+2,0x002f,16);
        byte2ascii(currentChannel,2+7);
       } else {
        // "P:xxxx    K:xx  "
        ee_readString(buffer+2,0x003f,16);
       }

       // Rellenamos Ident del proveedor
       byte2ascii(ee_read(addr),2+2);
       byte2ascii(ee_read(addr+1),2+4);

       // Rellenamos indice de la clave
       byte2ascii(currentKey,2+12);
    } else
    if (p1<0x08){
      if (keyExist(currentProvider,currentKey)){
        // indice inicial
        i=(p1-4)<<2;
        // indice final
        n=i+4;
        // convertir a ascii
        for (j=2; i<n; i++, j+=3){
          byte2ascii(Key[i],j);
        }
      }
    } else {
      // Añadimos información del último evento
      byte2ascii(ppv_record[EVENT_ID1],2);
      byte2ascii(ppv_record[EVENT_ID2],4);
      byte2ascii(ppv_record[SESSION_ID],6);
    }
  } // mode info ends

  // Comprobar multi EPG
  // Nos aseguramos que el currentProvider es
  // distinto de cero no sea que mandemos SECA
  // cuando p1 = 1
  if(multiEPG && currentProvider)
  {
    // Nos pide el 1 -> mandar el current
    if(p1 == 1){
      p1 = currentProvider;
    }
    // Nos pide el actual -> mandar el 1
    else if (p1 == currentProvider){
      p1 = 1;
    }
  }

  addr=getProvAddr(p1);

  // Ident
  ee_readString(buffer,addr,2);

  if (!infoMode){
    // Provider name en buffer[2..18]
    ee_readString(buffer + 2, addr + EE_ENT_NAME, 16);
    // PPUA + Fecha
    ee_readString(buffer+(2+16),addr + EE_ENT_PPUA,4+2);
  }
}


u16 getWord(u08 *src){
// return ((u16)(*src++<<8))+*src;
//  return ((u16)(*src<<8))|*(++src);
 return *((u16 *)(src));
}

u08 isCustWPActive(u08 nanof0_idx, u08 custwp)
{
  return rightShift(buffer[(nanof0_idx + 32) - rightShift(custwp,3)],custwp&7)&1;
}

/*------------------------------------------------------------------------
|
| Comprobación de eventos de PPV.
|
| Solo se dan de alta con la ins 32. Al dar de alta, la sesión se pone a 0xFF
| Cuando se recibe el evento con el nano 31 de 3C, se comrpueba que el evento
| esta en la lista. Si es así, se devuelve la sesion. Si no es asi, 0xFF
+-------------------------------------------------------------------------*/
u08 CheckEventPPV(u08 darDeAlta)
{
  u08 ubCnt;
  u08 *pRecords = PPVRecords;

  for( ubCnt = 0; ubCnt < NUM_PPV_EVENTS; ubCnt += 3)
  {
    //if((*(pRecords + ubCnt) == ppv_record[EVENT_ID1]) && (*(pRecords + ubCnt + 1) == ppv_record[EVENT_ID2]))
    u08 *pCurrentRecord = pRecords + ubCnt;

    if ( *((u16 *)pCurrentRecord )==*((u16 *)&ppv_record[EVENT_ID1]))
    {
      if (!darDeAlta)
      {
        //*(pRecords + ubCnt + 2) = ppv_record[SESSION_ID];
        pCurrentRecord[2] = ppv_record[SESSION_ID];
      }
      //return(*(pRecords + ubCnt + 2));
      return pCurrentRecord[2];
    }
  }

  if(darDeAlta)
  {
    if(posPPVRecs == NUM_PPV_EVENTS)
    {
      posPPVRecs = 0;
    }

    pRecords+=posPPVRecs;
    *pRecords++ = ppv_record[EVENT_ID1];
    *pRecords++ = ppv_record[EVENT_ID2];
    *pRecords++ = 0xFF;
    posPPVRecs+=3;
  }

  return(0xFF);
}

//
// Cuando se llama se cuenta con sb1 = 0x90, sb2 = 0x00
//
void processNanos(u08 p1,u08 ECMorEMM)
{
 u08 nano;
 u08 idx=0;
 u08 rightsAdquired = 0;
 u08 nanos_13_31 = 1;
 u08 pnanos = 1; // processed nanos
 u08 nanoActualizado;
 u16 addr;
 u08 cnt_n13 = 0; // nanos 0x13 procesados, para autopbm
 u08 nano_2C = 1; // controla el bloqueo de las X
 u08 ppuaNotChanged = 1;


 // Lee la fecha de expiración del proveedor para los nanos 0x22 y 0x27
 u16 subsDate;
 addr=getProvAddr(p1);
 ee_readString(auxDate, addr + EE_ENT_SUBS_DATE, 2);
 subsDate = getWord(auxDate);

 while (1){

  // Por si la cambiamos dentro del bucle
  addr = getProvAddr(p1);
  nano = buffer[idx];

  if (nano==0x82){
    return;
  } else

  // ECM nanos - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if (ECMorEMM){

  if (nano==0x2c){
    // x channel
    nano_2C =  decodeX;

    if(nanos_13_31 && !rightsAdquired)
    {
      sb2 = 0x1A;
      return;
    }
  } else

  if (nano==0x04 || nano==0x19){
    // free the channel o preview mode
    rightsAdquired = 1;
  } else

  if (nano==0x27 && !rightsAdquired){
    // test card expiration date

    if (getWord(&buffer[idx+1])>subsDate){
      sb1=0x93; sb2=0x01;
      return;
    }

    ppv_record[SESSION_DATE1] = buffer[idx+1];
    ppv_record[SESSION_DATE2] = buffer[idx+2];
  } else

  if (nano==0x13 && !rightsAdquired && nanos_13_31){
    // test channel boundle identifier
    u08 pbm,bid;
    if ((bid=buffer[idx+1])<64){
      addr +=  (EE_ENT_PBM + 7) - rightShift(bid,3);
      pbm=ee_read(addr);
      if (rightShift(pbm,bid&7)&1){
        RED_INFO_LED_OFF;
        rightsAdquired=1;
        currentChannel=bid;
      } else {
        #ifdef AUTOPBM
        if(ee_read(EE_FLAGS) & FLG_AUTOPBM){
          // Channel boundle no activo
          // Algoritmo de autoactualizacion si es el primer 0x13
          RED_INFO_LED_ON;

          if (cnt_n13==0){
            // almacenar la cuenta de apariciones del mismo bid seguido
            repeat_bid = (last_bid==bid)? (repeat_bid+1) : 0;
            // si el bid no cambia en tres ecm seguidas->autoPBM
            autoPBM = (repeat_bid==2);
            last_bid = bid;
           }

           if (autoPBM){
             u08 tmp;
             tmp=pbm|rightShift(0x80,7-(bid&7));
             if(tmp!=pbm)
               ee_write(addr,tmp);
             autoPBM=0x00;
             repeat_bid=0x00;
           }
        }
        #endif
      } // end of else
    }
    cnt_n13++;
  } else

  if (nano==0x31 && !rightsAdquired && nanos_13_31){

//    ppv_record[EVENT_ID1] = buffer[idx+1];
//    ppv_record[EVENT_ID2] = buffer[idx+2];
//    ppv_record[SESSION_ID] = buffer[idx+3];

    _memcpy(&ppv_record[EVENT_ID1], &buffer[idx+1], 3);

    // Si el evento ya esta en la lista o el deco no es oficial
    // podremos decodificar.
    if(decoNoOfi || (CheckEventPPV(FALSE) != 0xFF))
      nanos_13_31 = 0;    // Ya no es necesario verificar el 0x13, 0x31

  } else

  if (nano==0xD1){

    if ((rightsAdquired || !nanos_13_31) && (nano_2C)){
      u08 *ptr;
      ptr=&buffer[idx+1];
      decrip(Key,ptr);
      decrip(Key,ptr+8);
      _memcpy(CW,ptr,16);
    } else{
      // rights no adquired
      sb1=0x93; sb2=0x02;
      return;
    }
  }

  } // end of ECM nanos


  // EMM nanos - - - - - - - - - - - - - - - - - - - - - - - - - - -

  else {
  // Partimos de que actualizaremos los datos que vienen en el nano
  nanoActualizado = TRUE;
  sb1 = 0x97;
  if (nano==0x21){
    // update expiration date
    nanoActualizado = ee_updateString(addr + EE_ENT_SUBS_DATE, &buffer[idx+1], 2);
  } else

  if ((nano==0x22) && ppuaNotChanged){
    // test card expiration date
    if (getWord(&buffer[idx+1])>subsDate){
        sb1=0x93; sb2=0x01;
        return;
    }
  } else

  if (nano==0x41){
    // update PPUA
    if (ee_updateString(addr + EE_ENT_PPUA,&buffer[idx+1],4)){
       sb1 = 0x90;
       sb2 = 0x19; // 90 19
       return;
    }
  } else

  if (nano==0x80){
    // update PBM
    sb1 = 0x90;
    if ((ee_read(EE_FLAGS) & FLG_LOCKPBM)){
      if (!ee_updateString(addr + EE_ENT_PBM, &buffer[idx+1],8))
      {
        sb2 = 0x17; // 90 17: Update not needed
        return;
      }
      else
        continue;
    } else {
      sb2 = 0x15; // 90 15: Not allowed
      return;
    }
  } else

  if (nano==0x90 || nano==0x91){
    // update primary or secondary key
    nanoActualizado = updateKey(idx,p1,nano&0x01);
  } else

  if (nano==0xF0){
    // CustWP-Bitmap
    u08 custwp;

    addr += (EE_ENT_PPUA + 3);
    custwp = ee_read(addr);

    if (!isCustWPActive(idx, custwp)){
      // Si no esta activo y es distinto a cero -> 90 09

      #ifdef AUTOPPUA
      // PPUA Autoupdate: autoactualiza el custwp ( + 46 bytes)
      u08 cnt;

      cnt = ee_read(EE_FLAGS);
      if(cnt & FLG_AUTOPPUA){
        cnt = custwp;
        do{
           cnt++;
           if (isCustWPActive(idx,cnt)){
             // Encontrado un CustWP Activo! Actualizando PPUA!
             ee_write(addr,cnt);
             // enciende led rojo, se apagara en la proxima 0x3c
             RED_INFO_LED_ON;
             cnt = custwp; // forzar la salida
             ppuaNotChanged = 0;
           }
        } while (cnt!=custwp);
      }

      #endif
      sb1 = 0x90;
      sb2 = 0x09;
      return;
    }
  } // end of nano f0

  // Nano no tratado
  else
  {
    nanoActualizado = FALSE;
  }

  if (!(nanoActualizado))
  {
    sb2 |= pnanos;
  }
  pnanos <<= 1;

  } // end of EMM nanos



  // next nano
  nano>>=4;
  if (nano>0xC) nano=(nano-0x0b)<<3;
  idx+=(nano+1);
 }

}

#ifdef SERIAL
void SerialSend(u08 *aBuffer, u08 ubSize)
{
  do
  {
    loop_until_bit_is_set(USR, UDRE); // esperar hasta poder mandar otro byte
    outp(*aBuffer++, UDR);
  } while(--ubSize);
}
#endif



u08 checkKeys(u08 p1, u08 p2)
{
  u08 tmp, err = 0;

  p2 &= 0x0f;

  currentKey = p2;

  tmp = keyExist(p1, p2);

  if (tmp==0x00){
    // No existe clave primaria 90 1D
    err = 0x1D;
   } else
   if (tmp==0x50 && (p1 & 0x10)){
     // No existe clave secundaria 90 1F
     err=0x1F;
   }

   sb2=err;
   return(err);
}

#ifdef MODO_LOG
#include "log.h"
#endif

int main(void)
{
  u08 mentir;
  // cla,inst,p1,p2,p3
  u08 header[5];

  u08 lastInst=0; // stores the last processed instruction
  // Mascara con valor 0x00 si error; 0xFF si no error
  // Después de contestar una INS se hace lo siguiente,
  // lastInst = header[INS] & instErr
  u08 instErr;
  // keyleds:
  // - 0: always off (low power supply decos) (PIN:9999)
  // - 1: show operation key in use (PIN:9998)
  // - 2: show current provider index (PIN:9997)
  u08 keyleds;
  u08 numProviders;
  u08 numProvidersLog;
  u08 tmp;

  // Permite utilizar el phoenix para realizar operaciones de I/O en la eeprom externa.
  u08 support_phoenix=0x01;

  // Desactiva el comparador analogico :P
  sbi(ACSR,7);

  // Configura Puertos B y D

  // El puerto B    :
  //    - leds de colores (pins 2,3,4)
  //    - i/o (pin 6)

  outp(0x1C,DDRB);    // 0b00011100
  outp(0x1C,PORTB);   // 0b00011100

  // El puerto D    :
  //    - KeyLeds (pins 0,1,2,3)

#ifndef SERIAL
  outp(0x0F,DDRD);   // 0b00001111
  outp(0x0F,PORTD);  // 0b00001111
#else
  // UART
  // Se utiliza el PD1
  outp(0x08, UCR);  // Solo Tx
  outp(UBRR_VALUE, UBRR);
#endif



  /*-----------------------------------------------------------------------------
  | Detección modo Apollo:
  |
  | Se comprueba el estado del bit 7 y bit 5 del puerto B. Si el 7 está a 0 y
  | el 5 a 1, alguien los ha cambiado (FunStudio) -> modo Apollo
  |
  +------------------------------------------------------------------------------------*/

  // init eeprom externa
  extee_init();


  if(0x80 == (inp(PINB) & 0xA0))
  {
    sbi(DDRB,6);
    sbi(PORTB,6);
    RED_INFO_LED_ON;
    ptrSend    = &SendA;
    ptrReceive = &ReceiveA;
    #ifdef SERIAL
    modePhoenix = 0;
    #endif
    cbi(PORTB,6);
  }
  else
  {
    // Modo Phoenix
    ptrSend    = &Send;
    ptrReceive = &Receive;

    // Leer valor del ETU
    tmp = ee_read(EE_ONE_ETU);
    // Entre 340 y 390
    if((tmp < 0x54) || (tmp > 0x86))
      tmp = 0x74;

    // Inicializar contadores
    outp(0x01, OCR1AH);
    outp(tmp,  OCR1AL);
    outp(0x09, TCCR1B);

    // Enviamos ATR
    ORANGE_INFO_LED_ON;
    sendString(ATR,16);
    ORANGE_INFO_LED_OFF;
  }

  numProvidersLog = numProviders = ee_read(EE_NUM_ENTETIES) - 1;
  tmp = ee_read(EE_FLAGS);

  // Para los decos sin PIN. Permite bloquear los canales X
  decodeX   = tmp & FLG_BLOQUEAR_TX;
  multiEPG  = tmp & FLG_MULTI_EPG;
  decoNoOfi = tmp & FLG_DECO_NO_OFICIAL;
  mentir 		= !decoNoOfi;

  keyleds=ee_read(EE_KEYLEDS);

  #ifdef MODO_LOG
  //entraremos en modo log en la próxima instrucción si el primer byte != 0
  modo_log = tmp & FLG_AUTOLOG;
  leer_config_log(numProvidersLog);
  #endif

  while (1){

    // status word by default
    sb1=0x90;
    sb2=0x00;

    instErr = 0x00;

    GREEN_INFO_LED_OFF;     // apaga 'busy' led
    receiveString(header, 5);
    GREEN_INFO_LED_ON;      // enciende 'busy' led

    #ifdef SERIAL
    if(modePhoenix)
    {
      SerialSend(header,5);
    }
    #endif

    if (header[CLA]!=0xC1){
      // Clase no soportada: 6E 00
//      instErr = 0;
      sb1=0x6E;
      goto TX_Data;
    }

    if (header[INS]&1){
      // El primer bit de la inst debe ser cero
//      instErr = 0;
      sb1=0x6D;
      goto TX_Data;
    }

    if (!header[LEN] || header[LEN]>MAX_COMMAND_LENGHT){
      // Longitud de entrada incorrecta: 67 00
//      instErr = 0;
      sb1=0x67;
      goto TX_Data;
    }

    instErr = 0xFF;
    (*ptrSend)(header[INS]);  // Enviar ACK

    #ifdef SERIAL
    if(modePhoenix)
    {
      TX_SERIAL(header[INS]);
    }
    #endif

    if (header[INS]&2){
      // CARD->CAM
      fillBuffer(0,MAX_COMMAND_LENGHT,0xFF);
    } else {
      // CAM->CARD
      receiveString(buffer, header[LEN]);

      #ifdef SERIAL
      if(modePhoenix)
        SerialSend(buffer,header[LEN]);
      #endif

      #ifdef MODO_LOG
      // salvamos el buffer para el log
      _memcpy(&log_buffer[5],buffer,header[LEN]);
      #endif
    }

    /*-----------------------------------------------------------------------------
    | En principio, el tratar esta posibilidad aquí no debe producir problemas
    | Con !support_phoenix nos aseguramos poder mandar todas las direcciones
    | para las INS 20 y 22
    +-----------------------------------------------------------------------------*/
    if (((header[P1] & 0x0F) > numProviders) && (!support_phoenix)){
    // proveedor no soportado 90 04
      sb2=0x04;
      goto TX_Data;
    }


    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x34: Especificación de la Petición de Datos
    if (header[INS]==0x34){
      // Sólo soportamos dos peticiones:
      //   00 00 00 : Provider Package Bitmap Records
      //   04 00 01 : SECA Startup Records
      // La petición se almacena en Comando34


      if (header[LEN]!=3){
        // Longitud Incorrecta de la entrada: 67 00
        instErr = 0;
        sb1=0x67;
        goto TX_Data;
      }

      _memcpy(Comando34,buffer,3);
//      lastInst=header[INS];
    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x32: Petición de Fecha
    if (header[INS]==0x32){

      if (header[P2]){
        // valor de p2 incorrecto (p2!=0) 94 02
        instErr=0;
        sb1=0x94;
        sb2=0x02;
        goto TX_Data;
      }


      if (lastInst!=0x34){
        // Instrucción previa incorrecta 90 14
        instErr=0;
        sb2=0x14;
        goto TX_Data;
      }

      // Se puede volver a llamar a una ins 0x32 tras una ins 0x32 correctamente procesada.
      lastInst=0x34;
      // Devolvemos "sin datos" por defecto
      buffer[0]=0x04;

      // Comando 00 - - - - - - - - - - - - - - - - - - - -
      if (Comando34[0]==0x00){
        // Comando 00
        // Datos:  00 00 -> Provider Package Bitmap Records

        if (header[LEN]<0x0A){
          // No cabe, mandar 0x03
          buffer[0]=0x03;
        } else {
          if (header[P1]){
            // ok, todos menos SECA
            buffer[0]=0x83;

            #ifdef AUTOPBM
            if(ee_read(EE_FLAGS) & FLG_AUTOPBM)
            {
            	buffer[0x0A-2]= 0xFE;
          	}
            else
            {
            #endif
            	ee_readString(buffer + 1, getProvAddr(header[P1]) + EE_ENT_PBM, 8);
            #ifdef AUTOPBM
          	}
            #endif
            buffer[0x0A-1]=0x04;
          }
        }
      } else
      // Comando 01 - - - - - - - - - - - - - - - - - - - -
      if(Comando34[0]==0x01){
      	// Comando 01
      	// Enviamos PPV credit record falso, con 8 Jetons
        _memcpy(buffer,ppv_credit_record,10);

      } else
      // Comando 03 - - - - - - - - - - - - - - - - - - - -
      if (Comando34[0]==0x03){
        // Comando 03
        // Datos:  xx xx -> Provider PPV Records (xx xx = EventID)
        if(header[LEN]<0x0e){
          buffer[0] = 0x03;
        } else {
          ppv_record[EVENT_ID1] = Comando34[1];
          ppv_record[EVENT_ID2] = Comando34[2];

          if ( ee_read(EE_FLAGS) & FLG_AUTOPPV )
          {
            ppv_record[SESSION_ID] = CheckEventPPV(TRUE);
            _memcpy(buffer,ppv_record,14);
          }
          needPin = FALSE;
        }
      } else
      // Comando 04 - - - - - - - - - - - - - - - - - - - -
      if (Comando34[0]==0x04){
        // Commando 04
        // Datos:  00 00 -> SECA PPV Records
        //         00 01 -> SECA Startup Records
        //         00 02 -> SECA Records de Activación
        if (Comando34[2]==0x01){
          // SECA Startup Records
          if (header[LEN]<0x0d){
            // No cabe, mandar 0x03
            buffer[0]=0x03;
          } else {
            if (!header[P1]){ // solo a SECA
              buffer[0]=0xb2;
              ee_readString(buffer + 1, EE_STARTUP_STR, 11);
              buffer[0x0D-1]=0x04;
            }
          }
        }
      } // fin comando 04
    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x0E: Número de serie de la tarjeta
    if (header[INS]==0x0E){

      fillBuffer(0,2,0x00);
      // numero de serie esta en SECA.UA
      ee_readString(buffer + 2, EE_NUM_TARJETA, 6);
      support_phoenix=0x00;
    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x16: Enumeración de Proveedores
    if (header[INS]==0x16){
      u16 tmp16;

      fillBuffer(0,5,0x00);
      tmp16 = 0xffff>>(15 - numProviders);
      buffer[2] = tmp16>>8;
      buffer[3] = tmp16&0x00ff;

      //tmp=numProviders + 1; //número de proveedores + seca
      //buffer[2]=rightShift(0xff,16-tmp);
      //buffer[3]=rightShift(0xff,(tmp<8?(8-tmp):0));
    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x12: ProviderID (Ident)
    if (header[INS]==0x12){
      setupInfo(header[P1],header[LEN]);
    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x3C: ECM ControlWords encriptadas
    if (header[INS]==0x3C){
      // p1 = provider number & pk-sk
      // p2 = key index & superencription

      // Tratamos el problema del número de proveedores en deco oficial
  		if(mentir)
    		numProviders = 1;

      needPin = TRUE;
      pinChecked = FALSE;
      // actualizar variables modo info
      currentProvider=header[P1]&0x0F;
//      currentKey=header[P2]&0x0F;
      currentChannel=0;
/*

      tmp = keyExist(header[P1],currentKey);

      if (tmp==0x00){
        // No existe clave primaria 90 1D
        sb2=0x1D;
        goto TX_Data;
      } else
      if (tmp==0x50 && (header[P1]&0x10)){
        // No existe clave secundaria 90 1F
        // excepcion: devuelve los 0x10 primeros bytes para 0x3a
        // Atencion: Ahora devuelve relleno. Cuidadin!
        sb2=0x1F;
        goto TX_Data;
      }
*/
      // comprueba existencia de claves
      if(checkKeys(header[P1], header[P2]))
        goto TX_Data;


      decode(header[P2],header[LEN]);

      if (signatureCheck(header[P1],header[LEN])){
        RED_INFO_LED_OFF;
        processNanos(header[P1],kECM);
      } else {
        // 90 02: Signature fails
        sb2=02;
        RED_INFO_LED_ON;
      }

    #ifndef SERIAL

      // info en KeyLeds

      // Suponemos keyleds==0
      tmp=0xF0;
      if (keyleds==1){
        tmp=currentKey;
        keyleds=2;
      } else
      if (keyleds==2){
        tmp=currentProvider;
        keyleds=1;
      }
      outp(~(tmp),PORTD);

    #endif


    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x3A: ECM ControlWords desencriptadas
    if (header[INS]==0x3A){

      // Verificar inst, anterior.
      if (lastInst!=0x3C){
        sb2=0x14;
        goto TX_Data;
      }

      if (header[P1]!=0){
        sb2=0x04;
        goto TX_Data;
      }

//      lastInst=header[INS];
      // enviar CW desencriptadas
      _memcpy(buffer,CW,16);
    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x30: Transacciones protegidas por PIN
    if (header[INS]==0x30){
//      lastInst=header[INS];
      //  Introduccion/modificacion de PIN
      //   - El PIN se almacena en eeprom externa.
      //   - Utilizo los dos ultimos bytes de la UA
      //    del proveedor con indice 1 (sigue a SECA).
      if (buffer[14]==0x27){
        // Apagar Keyleds?

        if (buffer[15] >= 0x0d && buffer[15] <= 0x0f){
          ee_write(EE_KEYLEDS, 0x0f-buffer[15]);
        }

/*
        // Apagar Keyleds?
        if (buffer[15]==0x0f){
          ee_write(EE_KEYLEDS,0);
        }

        // Keyleds muestran indice de la clave en uso
        if (buffer[15]==0x0e){
          ee_write(EE_KEYLEDS,1);
        }

        // Keyleds muestran indice del proveedor actual
        if (buffer[15]==0x0d){
          ee_write(EE_KEYLEDS,2);
        }
*/
      } else

      if (buffer[14]==PIN_NPRO_H)
      {
        if(buffer[15]==PIN_NPRO_L) {
          mentir = !mentir;
          numProviders = (mentir)? 1 : numProvidersLog;
        } else
        if(buffer[15]==PIN_MEPG_L) {
          multiEPG=!(multiEPG);
        }
      }

      #ifdef MODO_LOG
      else tratar_PINS_log( buffer[14], buffer[15], numProvidersLog );
      #endif

      /*-----------------------------------------------------------------------------
      | Bloqueo de taquillas oficial:
      |   Una vez que se bloquea una taquiila, el deco pedirá PIN a la hora de la
      |   compra aún cuando esa taquilla no esté bloqueada. En una taquilla bloqueada,
      |   lo pide 2 veces: a la hora de entrar en la taquilla y a la hora de la compra.
      |
      | Lo que intentamos es simular el bloqueo de taquillas evitando la petición del
      | PIN, a la hora de la compra, si esa taquilla no está bloqueada. En una bloqueada
      | lo seguirá pidiendo 2 veces.
      |
      | A la hora de realizar la compra o volver a una taquilla ya comprada, el deco no
      | nos da datos suficientes para poder saber en que situación estamos. El sistema
      | que hemos adoptado garantiza, al menos, una petición de PIN cuando se entre en
      | una taquilla bloqueada, ya sea para el proceso de compra o si cambiamos a una
      | taquilla bloqueada con un evento de compra ya activado. En contra partida, en alguna
      | situación, puede darse el caso de que se pida PIN en el proceso de compra de una
      | taquilla sin bloquear :-)
      |
      | Variables que controlan la situación:
      |   needPin : Se activa al inicio, en cada INS 3C, si se introduce in PIN correcto
      |             o si en la comprobación del PIN se encontraba sin activar needPIN o
      |             pinChecked.
      |             Se desactiva con la INS 32, comando 4 (compra PPV)
      |
      |   pinChecked : Se activa si se introduce un PIN correcto o si en la comprobación
      |                del PIN se encontraba sin activar needPIN o pinChecked.
      |                Se desactiva al inicio y en cada INS 3C.
      |
      | Se comprueba el PIN si cualquiera de las dos variables se encuentran
      | activas en el momento de la petición del mismo.
      +------------------------------------------------------------------------------------*/

      if ((pinChecked) || (needPin)){
        if (mem2eeprom_cmp(EE_PIN,&buffer[14],2)==0){
          // ultimo PIN validado = PIN almacenado   ?
          if (mem2eeprom_cmp(EE_PIN,&buffer[6],2)==1){
            // modificación del PIN
            ee_writeString(EE_PIN,&buffer[14],2);
          }
          else {
            sb2=0x10; // no validar PIN: 9010
          }
        } else {
        needPin = pinChecked = TRUE;
        }
      }
      else {                            // Aunque parezca mentira
        needPin = pinChecked = TRUE;    // esta línea ocupa -2 bytes !!??
      } // fin comprobación pin

    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x40: EMM Manejo de los Abonos
    if (header[INS]==0x40){

      // comprobar que utiliza MK
      if (header[P2]&0x08){
        sb2=0x13;
        goto TX_Data;
      }

/*
      tmp = keyExist(header[P1], header[P2]&0x0f);

      if (tmp==0x00){
        // No existe clave primaria 90 1D
        sb2=0x1D;
        goto TX_Data;
      } else
      if (tmp==0x50 && (header[P1]&0x10)){
        // No existe clave secundaria 90 1F
        sb2=0x1F;
        goto TX_Data;
      }
*/
      // comprueba existencia de claves
      if(checkKeys(header[P1], header[P2]))
        goto TX_Data;

      if (header[P1]&0x0f){
        decode(header[P2],header[LEN]);

        if (signatureCheck(header[P1],header[LEN])){
          processNanos(header[P1],kEMM);
        } else {sb2=0x02;}
      } else { sb2=0x02; } // SECA siempre inaccesible a las EMM
    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x1A: Lectura de los Indices de claves
    if (header[INS]==0x1A){
      u08 k,cnt,pk_sk;
      // FF FF 0B 40 00
      ee_readString(buffer, EE_AVAIL_KEYS, 5);

      // Para comprobar las MK secundarias
      pk_sk = header[P1] | 0x10;
      for (cnt=0, tmp=5; cnt<16; cnt++){
        k=keyExist(pk_sk,cnt)|cnt;
        if (k&0xf0){
            buffer[tmp++]=k;
        }
      }
    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x5A: Lectura de claves encriptadas
    if (header[INS]==0x5A){

      // comprueba existencia de claves
      if(checkKeys(header[P1], header[P2]) == 0)
      {
        // Preparar 8*0x00
        fillBuffer(0,16,0x00);
        encript(Key,buffer);
      }

/*
      tmp = keyExist(header[P1],header[P2]&0x0f);

      if (tmp==0x00){
        // No existe clave primaria 90 1D
        sb2=0x1d;
      } else
      if (tmp==0x50 && (header[P1]&0x10)){
        // No existe clave secundaria 90 1F
        sb2=0x1F;
      } else {
        // Preparar 8*0x00
        fillBuffer(0,16,0x00);
        encript(Key,buffer);
      }
*/

    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x0A: Sistema de Funcionamiento de la Tarjeta
    if (header[INS]==0x0A){
//      lastInst=header[INS];

      _memcpy(buffer,version,TOTAL_VERSION);
    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x22: Lectura de eeprom
    // C1 22 P1 P2 P3
    // P1 Low Addr
    // P2 High Addr
    // P3 Bytes to read
    if ( (header[INS]==0x22) && support_phoenix){
      ee_readString(buffer,(((u16)header[P1])<<8)+header[P2],header[LEN]);
    } else
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // INSTRUCCION 0x20: Escritura en eeprom
    // C1 20 P1 P2 P3
    // P1 Low Addr
    // P2 High Addr
    // P3 Bytes to write
    if ( (header[INS]==0x20) && support_phoenix){
      RED_INFO_LED_ON;
      ee_writeString((((u16)header[P1])<<8)+header[P2],buffer,header[LEN]);
      RED_INFO_LED_OFF;
    } else {
    // UNKNOW INSTRUCTION
      instErr=0;
      sb1=0x6D;
    }

    TX_Data:

    #ifdef MODO_LOG
    if (modo_log && !support_phoenix){
      capturar_inst(header);
    }
    #endif

    if(header[INS]&2){
      // CARD->CAM
      sendString(buffer,header[LEN]);
      #ifdef SERIAL
      if(modePhoenix)
        SerialSend(buffer,header[LEN]);
      #endif
    }

    // Mandar status bytes
    sendString(&sb1, 1);
    sendString(&sb2, 1);

//    (*ptrSend)(sb1);
//    (*ptrSend)(sb2);

    #ifdef SERIAL
    if(modePhoenix)
    {
      TX_SERIAL(sb1);
      TX_SERIAL(sb2);
    }
    #endif

    lastInst = header[INS] & instErr;

    #ifdef MODO_LOG
    if (ins_capturadas){
        ORANGE_INFO_LED_ON;
    }
    #endif


 } // end of while(1){}


  return 0;
} // end of main()
