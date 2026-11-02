/**
  ******************************************************************************
  * @file    : HW_Ret_Code.h
  * @brief   : HW Driver layer - common return codes for every layer
  *
  * Single return-code header. Included by Driver, Module, App and System.
  * Vendor headers are never included here; standard type headers are
  * provided from here so that no layer pulls them in on its own.
  *
  * Profile: 8-bit MCU (STM8L151F3, 1 KB RAM) -> Ret_t is int8_t,
  * module-specific codes live in 64..127 (spec v2.0.3 §9).
  ******************************************************************************
  */
#ifndef __HW_RET_CODE_H__
#define __HW_RET_CODE_H__

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

/* Exported types ------------------------------------------------------------*/
typedef int8_t Ret_t;

/* Exported constants --------------------------------------------------------*/
/* 0 : success -------------------------------------------------------------- */
#define RET_OK                   ((Ret_t)0)

/* 1..3 : general state ----------------------------------------------------- */
#define RET_ERROR                ((Ret_t)1)
#define RET_BUSY                 ((Ret_t)2)
#define RET_TIMEOUT              ((Ret_t)3)

/* 4..9 : hardware detail --------------------------------------------------- */
#define RET_HW_NACK              ((Ret_t)4)
#define RET_HW_OVERRUN           ((Ret_t)5)
#define RET_HW_FRAME             ((Ret_t)6)
#define RET_HW_NOISE             ((Ret_t)7)

/* 10..19 : common ---------------------------------------------------------- */
#define RET_ERR_PARAM            ((Ret_t)10)
#define RET_ERR_NULL             ((Ret_t)11)
#define RET_ERR_NOT_INIT         ((Ret_t)12)
#define RET_ERR_STATE            ((Ret_t)13)
#define RET_ERR_FULL             ((Ret_t)14)
#define RET_ERR_EMPTY            ((Ret_t)15)
#define RET_ERR_CRC              ((Ret_t)16)
#define RET_ERR_RANGE            ((Ret_t)17)
#define RET_ERR_UNSUPPORTED      ((Ret_t)18)

/* 20..63 : reserved for common extension (do not use) ---------------------- */

/* 64..127 : module-specific (<PART>_ERR_<NAME> = RET_MODULE_BASE + n) ------- */
#define RET_MODULE_BASE          ((Ret_t)64)
#define RET_MODULE_MAX           ((Ret_t)127)

#endif /* __HW_RET_CODE_H__ */
/*****************************END OF FILE****/
