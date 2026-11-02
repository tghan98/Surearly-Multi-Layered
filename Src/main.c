/**
  ******************************************************************************
  * @file    : main.c
  * @brief   : Startup code - calls the three System_Main entry points only
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "System_Main.h"

/* Functions -----------------------------------------------------------------*/
void main(void)
{
    System_Main_EarlyInit();

    (void)System_Main_Init();

    while (1)
    {
        (void)System_Main_Run();
    }
}

/*****************************END OF FILE****/
