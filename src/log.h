/*********************************************************************
    Fichero:    Comm_log.h
    Autor:      Fun-o-Matic
    Fecha:      Mayo/Julio 2001
    Proposito:  Añadir rutinas LOG
    Compilador: AVR-GCC + Make2 de gcctest.zip
    Hardware:   FunCard v1.0
**********************************************************************/

#ifndef __LOG_H__
#define __LOG_H__

#include "types.h"

//LOG
//PINs utilizados
//#define PIN_KL_MODOLOG    8899      //desactiva el modo log

#define TIPO_LOG      0xC0            //Byte tipo log para FUNLOG
//#define BYTE_LOG(x) (x<<1)|TIPO_LOG|LEDS_LOG
#define ADDR_LOGCFG   (8192-(5*3 + 1))
#define MAX_EE_LEN    (0x2000-32-MAX_COMMAND_LENGHT)         //Límite espacio log
#define END_OF_LOG    0xFF    //Marca de fin de log

//u08 masklog[6];          //màscaras para el log=> 5 inst + 1 led
u08 log_buffer[5 + MAX_COMMAND_LENGHT + 3]; // montar datos a loggear

u08 mascaras[3*5]; // mascaras de log

u08 modo_log;      //estamos en modo log?

u08 ins_capturadas;  //número instrucciones capturadas
u16 iee;             //índice sobre eeprom externa
//si procede capturaremos bytes enviados y
//respuesta de la tarjeta a la instrucción


// Comprobar con la mascara:
// Si en algun nibble de la mascara encontramos "f"
// significa que no queremos que pese ese nibble en la
// comparacion con el byte de cabecera.

u08 checkMask(u08 idx)
{
 u08 cnt;
 u08 maskbyte;
 u08 headerbyte;

 for (cnt=0; cnt<5; cnt++){
   maskbyte =mascaras[idx+cnt];
   headerbyte = log_buffer[cnt];

   if ( (maskbyte&0xf0)==0xf0 ){
     // comprobar sólo nibble inferior
     headerbyte|=0xf0;
   }

   if ( (maskbyte&0x0f)==0x0f ){
     // comprobar solo el nibble superior
     headerbyte|=0x0f;
   }

   if (headerbyte!=maskbyte)
        return 0;

 }

 return (1);
}



//Realiza la comprobacion para captura de instruccion
//Captura la cabecera de la instrucción + datos + bytes status (CAM<->CARD),
//según la configuración del log.
void capturar_inst(u08 *header)
{
  u08 i,do_log, len = *(header + LEN);
  // Montamos la instruccion a logear
  u08 *ptr = log_buffer;

  _memcpy(ptr, header, 5);
  ptr+= 5;

  if(*(header + INS)&2){
    // CARD->CAM --> copiamos datos generados
    _memcpy(ptr,buffer,len);
  }

  ptr+= len;
  *ptr++ = sb1;
  *ptr++ = sb2;
  *ptr = END_OF_LOG;

  // comprobar mascara a capturar
  do_log = checkMask(0);
  // comprobar mascaras a no capturar
  for (i=5; i<15; i+=5){
    do_log &= (!checkMask(i));
  }

  //si tenemos la tarjeta en el phoenix, no capturar nada!
  if(do_log){
    len += (5 + 2 + 1);

    ORANGE_INFO_LED_ON;

    // quemamos los datos en eeprom
    ee_writeString(iee,log_buffer,len);
    iee+=(len-1);

    //incrementamos instrucciones capturadas
    ins_capturadas++;

    //Memoria llena?
    if (iee>=MAX_EE_LEN)
      modo_log=0;

  }
}


//Leemos la configuración guardada en la eeprom.
//Se llama al inicio y con el PIN de modo log.
//numprov=> proveedores en la eeprom.
//numMask=> máscara a leer.
void leer_config_log( u08 numProv) {
  //leemos configuración de la eeprom externa (SECA)
  // nota: numMask*5 optimizado por el compilador como numask<<2 + numMask
  //ee_readString(masklog, ADDR_LOGCFG + (numMask*5), 5);

  // Comprobamos con mascaras aprovecando memoria de buffer
  ee_readString(mascaras, (u16)ADDR_LOGCFG, 3*5);

  //averiguamos 1ª posición libre en eeprom
  iee=getProvAddr(numProv+1);
  //escribimios tipo de log para funlog
  ee_write( iee, TIPO_LOG );
  //Saltamos 5 por compatibilitat con versiones funlog.
  iee+=5;
  //inicializamos contador instrucciones capturadas
  ins_capturadas=0;
}

//función para tratar los pins
//b1,b2 => bytes de los pins
//np=> número de proveedores
void  tratar_PINS_log( u08 b1, u08 b2, u08 np){
  // PIN 8899 activa / desactiva el modo log.
  if ((b1==0x22) && (b2==0xC3)) {
      //si estamos en modo log, lo anulamos
      modo_log = !modo_log;
      if(modo_log)
        leer_config_log(np);
  }
}


#endif

