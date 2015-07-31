#ifndef __FUNSECA_H__
#define __FUNSECA_H__

#define TRUE  (1)
#define FALSE (0)


// Los Leds
#define RED_INFO_LED_ON          {cbi (PORTB,2);}
#define RED_INFO_LED_OFF         {sbi (PORTB,2);}
#define ORANGE_INFO_LED_ON       {cbi (PORTB,3);}
#define ORANGE_INFO_LED_OFF      {sbi (PORTB,3);}
#define GREEN_INFO_LED_ON        {cbi (PORTB,4);}
#define GREEN_INFO_LED_OFF       {sbi (PORTB,4);}

//
// ExtEEProm.h
//

//
// Corresponde a la EEProm beta2 de Funcard
//
#define EE_SIZE             (0x2000)
#define EE_ENTETY_SIZE      (0x0127)     // Tamaño de una Entety -> 295 bytes
#define EE_KEY_SIZE         (0x0008)
#define EE_VERSION          (0x0002)
#define EE_MENU_INFO        (0x000D)

#define EE_CONFIG_BYTE_FC   (0x0060)

#define EE_STARTUP_STR      (0x0061)
#define EE_AVAIL_KEYS       (0x006C)
#define EE_NUM_ENTETIES     (0x0073)
#define EE_NUM_TARJETA      (0x0087)

#define EE_PIN_FC           (0x01B2)

// El primer proveedor
#define EE_ENTETY_0         (0x0074)
// Offsets desde la base del proveedor
#define EE_ENT_NAME         (0x0003)
#define EE_ENT_UA           (0x0013)
#define EE_ENT_PPUA         (0x0019)
#define EE_ENT_SUBS_DATE    (0x001D)
#define EE_ENT_PBM          (0x001F)
#define EE_ENT_KEY_0        (0x0027)

#define EE_FLAGS            (8191)
#define EE_MASK_C           (EE_FLAGS  - 5)
#define EE_MASK_B           (EE_MASK_C - 5)
#define EE_MASK_A           (EE_MASK_B - 5)
#define EE_PIN              (EE_MASK_A - 2)
#define EE_KEYLEDS          (EE_PIN - 1)      // almacena el uso de los keyleds
#define EE_ONE_ETU          (EE_KEYLEDS  - 1)


// Los flags de la variable de configuración -> EE_FLAGS
#define FLG_AUTOLOG         (0x01)
#define FLG_AUTOPBM         (0x02)
#define FLG_AUTOPPUA        (0x04)
#define FLG_BLOQUEAR_TX     (0x08)
#define FLG_MULTI_EPG       (0x10)
#define FLG_LOCKPBM         (0x20)
#define FLG_DECO_NO_OFICIAL (0x40)
#define FLG_AUTOPPV         (0x80)

#endif

