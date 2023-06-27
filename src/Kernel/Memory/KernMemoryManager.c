#include "../../Common/Memory/KernMem.h"
#include "../../Common/Memory/KernEfiMem.h"
#include "../../Common/Memory/KernMemoryManager.h"
#include "../../Common/Assert/KernAssert.h"
#include "../../Common/Util/KernRuntimeValues.h"
#include "../../Common/Util/KernString.h"
#include "../../Common/Drivers/IO/serial.h"

#include <Uefi.h>

#include <Library/UefiLib.h>

VOID
AllocateBitmap (
  IN EFI_KERN_MEMORY_MAP  *MemoryMap,
  IN UINTN                __SizeNeeded OPTIONAL
  )
{
  EFI_MEMORY_DESCRIPTOR  *Desc           = MemoryMap->MemoryMap;
  BOOLEAN                SizeUnspecified = __SizeNeeded == 0;

  while (
         (UINTN)Desc < ((UINTN)MemoryMap->MemoryMap + MemoryMap->MemoryMapSize) &&
         Desc->PhysicalStart < MEMORY_UPPER_BOUNDARY &&
         Desc->PhysicalStart < 2147483648
         )
  {
    if (SizeUnspecified) {
      BitmapSize += DIV_ROUNDUP (Desc->NumberOfPages, 8);
    }

    Desc = (EFI_MEMORY_DESCRIPTOR *)NEXT_MEMORY_DESCRIPTOR (
                                                            Desc,
                                                            MemoryMap->DescriptorSize
                                                            );

    if (SizeUnspecified) {
      continue;
    }

    if (
        (Desc->Type != EfiConventionalMemory) &&
        (Desc->Type != EfiLoaderCode) &&
        (Desc->Type != EfiLoaderData) &&
        (Desc->Type != EfiBootServicesData)
        )
    {
      continue;
    }

    if (CALC_SIZE_OF_FRAME (Desc) >= __SizeNeeded) {
 #ifdef DEBUG_MEMORY
      WriteSerialStr ("[DEBUG::MEMORY]: FOUND SUITABLE PAGE TO STORE BITMAP AT: ");
      WriteSerialStr (__DecimalToHex (Desc->PhysicalStart, TRUE));
      WriteSerialStr ("\n");
 #endif

      Bitmap = (UINT8 *)Desc->PhysicalStart;

      break;
    }
  }

  if (SizeUnspecified) {
 #ifdef DEBUG_MEMORY
    WriteSerialStr ("[DEBUG::MEMORY]: BITMAP REQUIRES SIZE OF: ");
    WriteSerialStr (_KernItoa (BitmapSize));
    WriteSerialStr (" BYTES!\n");
 #endif

    return AllocateBitmap (MemoryMap, BitmapSize);
  }
}

VOID
KernCreateMMap (
  IN EFI_KERN_MEMORY_MAP  *MemoryMap
  )
{
  ASSERT (MemoryMap != NULL);
  ASSERT (MemoryMap->MemoryMap != NULL);

  EFI_MEMORY_DESCRIPTOR  *Entry = MemoryMap->MemoryMap;

  AllocateBitmap (MemoryMap, 0);

  if (Bitmap == NULL) {
    kprint ("[DEBUG::KernMemoryManager]: FAILED TO ALLOCATE BITMAP!\n");

    return;
  }

  while (
         (UINTN)Entry < ((UINTN)MemoryMap->MemoryMap + MemoryMap->MemoryMapSize) &&
         Entry->PhysicalStart < MEMORY_UPPER_BOUNDARY &&
         Entry->PhysicalStart < 2147483648
         )
  {
    BOOLEAN  PageIsFree = FALSE;

    if (
        (Entry->Type == EfiConventionalMemory) ||
        (Entry->Type == EfiLoaderCode) ||
        (Entry->Type == EfiLoaderData) ||
        (Entry->Type == EfiBootServicesData) ||
        (Entry->Type == EfiBootServicesCode)
        )
    {
      if (Entry->PhysicalStart == (UINTN)Bitmap) {
        goto exit_bitmap_set;
      }

      PageIsFree = TRUE;

      BitmapSize += DIV_ROUNDUP (Entry->NumberOfPages, 8);

      for (
           UINTN Index = Entry->PhysicalStart;
           Index < Entry->PhysicalStart + Entry->NumberOfPages * KERN_SIZE_OF_PAGE;
           Index += KERN_SIZE_OF_PAGE
           )
      {
        UINTN  ByteFree = Index / KERN_SIZE_OF_PAGE / 8;
        UINTN  Bit      = (Index / KERN_SIZE_OF_PAGE) % 8;

        Bitmap[ByteFree] |= (1 << Bit);
      }
    }

exit_bitmap_set:

 #ifdef DEBUG_MEMORY
    WriteSerialStr ("[DEBUG::MEMORY]: PAGE AT ADDRESS RANGE: [");
    WriteSerialStr (__DecimalToHex (Entry->PhysicalStart, TRUE));
    WriteSerialStr (" - ");
    WriteSerialStr (__DecimalToHex (Entry->PhysicalStart + Entry->NumberOfPages * KERN_SIZE_OF_PAGE - 1, TRUE));
    WriteSerialStr ("] IS ");
    WriteSerialStr (PageIsFree ? "[[FREE]]\n" : "[[RESTRICTED]]\n");
 #endif

    Entry = (EFI_MEMORY_DESCRIPTOR *)NEXT_MEMORY_DESCRIPTOR (Entry, MemoryMap->DescriptorSize);
  }
}
