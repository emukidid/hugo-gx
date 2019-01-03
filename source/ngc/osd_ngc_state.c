/*
 *  osd_ngc_state.c
 *
 *  FAT and Memory Card Savestate files managment
 *
 ***************************************************************************/

#include "pce.h"
#include "osd_ngc_machine.h"
#include "font.h"
#include "saveicon.h"
#include "filesel.h"

#ifndef HW_RVL
#include "dvd.h"
#endif

#define SAVESIZE 0x4000

// Support for MemCards
// libOGC System Work Area
static u8 SysArea[CARD_WORKAREA] ATTRIBUTE_ALIGN (32);
static card_dir CardDir;
static card_file CardFile;
static card_stat CardStatus;

// DMA Transfer Area.
// Must be 32-byte aligned.
// 64k SRAM + 2k Icon
static u8 savebuffer[SAVESIZE] ATTRIBUTE_ALIGN (32);
char rom_filename[MAXJOLIET];

extern unsigned long ROMCRC32;
extern unsigned char *WRAM;
extern void ResetSound();
extern unsigned int savetimer;



/****************************************************************************
 * SDCARD Access functions
 *
 * We use the same buffer as for Memory Card manager
 * Function returns TRUE on success.
 *****************************************************************************/
static int FAT_ManageFile(char *filename, int direction)
{
  char pathname[MAXPATHLEN];
  int done = 0;
  int filesize;
  
  if (!fat_enabled) return 0;

  // first check if directory exist
  sprintf (pathname, "%s/saves", DEFAULT_PATH);

  DIR* dir = opendir(pathname);
  if (!dir) mkdir(pathname,S_IRWXU);
  else closedir(dir);

  // build complete SDCARD filename
  sprintf (pathname, "%s/%s", pathname, filename);

  // open file
  FILE *fp = fopen(pathname, direction ? "rb" : "wb");
  if (fp == NULL)
  {
    sprintf (filename, "Error opening %s", pathname);
    WaitPrompt(filename);
    return 0;
  }
  
  switch (direction)
  {
    case 0: // SAVING

      memcpy (&savebuffer[0], WRAM, 0x2000);
      filesize = 0x2000;

      // write buffer
      done = fwrite(savebuffer, 1, filesize, fp);
      if (done < filesize)
      {
        sprintf (filename, "Error writing %s", pathname);
        WaitPrompt (filename);
        return 0;
      }

      fclose(fp);
      sprintf (filename, "Saved %d bytes successfully", done);
      WaitPrompt (filename);
      return 1;
    
    case 1: // LOADING
    
      // read size
      fseek(fp , 0 , SEEK_END);
      filesize = ftell (fp);
      fseek(fp, 0, SEEK_SET);

      // read into buffer (32k blocks)
      done = fread(savebuffer, 1, filesize, fp);
      if (done < filesize)
      {
        sprintf (filename, "Error reading %s", pathname);
        WaitPrompt(filename);
        return 0;
      }
      fclose(fp);

      memcpy (WRAM, &savebuffer[0], 0x2000);
      ResetPCE();
      ResetSound();
      savetimer = 0;
            
      sprintf (filename, "Loaded %d bytes successfully", done);
      WaitPrompt (filename);
      return 1;
  }
  
  return 0; 
}

/****************************************************************************
 * MountTheCard
 *
 * libOGC provides the CARD_Mount function, and it should be all you need.
 * However, experience with previous emulators has taught me that you are
 * better off doing a little bit more than that!
 *
 * Function returns TRUE on success.
 *****************************************************************************/
int MountTheCard (u8 slot)
{
  int tries = 0;
  int CardError;
  *(unsigned long *) (0xcc006800) |= 1 << 13; // Disable Encryption
#ifndef HW_RVL
  uselessinquiry ();
#endif
  while (tries < 10)
  {
    VIDEO_WaitVSync ();
    CardError = CARD_Mount (slot, SysArea, NULL); // Don't need or want a callback
    if (CardError == 0) return 1;
    else EXI_ProbeReset ();
    tries++;
  }
  return 0;
}

/****************************************************************************
 * CardFileExists
 *
 * Wrapper to search through the files on the card.
 * Returns TRUE if found.
 ****************************************************************************/
int CardFileExists (char *filename, u8 slot)
{
  int CardError = CARD_FindFirst (slot, &CardDir, TRUE);
  while (CardError != CARD_ERROR_NOFILE)
  {
    CardError = CARD_FindNext (&CardDir);
    if (strcmp ((char *) CardDir.filename, filename) == 0) return 1;
  }
  return 0;
}

/****************************************************************************
 * ManageWRAM
 *
 * Here is the main SRAM Management stuff.
 * The output file contains an icon (2K), 64 bytes comment and the SRAM (64k).
 * As memcards are allocated in blocks of 8k or more, you have a around
 * 6k bytes to save/load any other data you wish without altering any of the
 * main save / load code.
 *
 * direction == 0 save, 1 load.
 ****************************************************************************/
extern int hugoromsize;

int ManageWRAM (u8 direction, u8 device)
{
  if (cart_reload || !hugoromsize) return 0;
 
  char filename[128];
 
  // clean buffer
  memset(savebuffer, 0, SAVESIZE);

  if (direction) ShowAction ("Saving WRAM ...");
  else ShowAction ("Loading WRAM ...");

  if (device == 0)
  {
    // FAT support
    sprintf (filename, "%s.hgo", rom_filename);
    return FAT_ManageFile(filename,direction);
  }

  // Memory CARD support
  char action[80];
  int CardError;
  unsigned int SectorSize;
  int blocks;
  char comment[2][32] = { {"Hu-go! 2.12 [WRAM]"}, {"ANY GAME"} };
  int outbytes = 0;
  int sbo;
  int state_size = 0;

  // First, build a filename
  sprintf(filename, "%08lX.hgo", ROMCRC32);
  sprintf(comment[1], "CRC : %08lX", ROMCRC32);

  // set MCARD slot nr.
  u8 CARDSLOT = device - 1;

  // Saving
  if (direction == 0)
  {
    // Build the output buffer
    memcpy (&savebuffer, &icon, 2048);
    memcpy (&savebuffer[2048], &comment[0], 64);
    state_size = 0x2000;
  }

  outbytes = 2048 + 64 + state_size;

  // Initialise the CARD system
  memset(&SysArea, 0, CARD_WORKAREA);
  CARD_Init("HUGO","00");

  // Attempt to mount the card
  CardError = MountTheCard(CARDSLOT);

  if (CardError)
  {
    // Retrieve the sector size
  CardError = CARD_GetSectorSize(CARDSLOT, &SectorSize);

    switch (direction)
    {
      case 0: // Saving
        // Determine number of blocks on this card
        blocks = (outbytes / SectorSize) * SectorSize;
        if (outbytes % SectorSize) blocks += SectorSize;

        // Check if a previous save exists
        if (CardFileExists (filename,CARDSLOT))
        {
          CardError = CARD_Open (CARDSLOT, filename, &CardFile);
          if (CardError)
          {
            sprintf (action, "Error Open : %d", CardError);
            WaitPrompt (action);
            CARD_Unmount (CARDSLOT);
            return 0;
          }

          int size = CardFile.len;
          CARD_Close (&CardFile);

          if (size < blocks)
          {
            // new size is bigger: check if there is enough space left
            CardError = CARD_Create (CARDSLOT, "TEMP", blocks-size, &CardFile);
            if (CardError)
            {
              sprintf (action, "Error Update : %d", CardError);
              WaitPrompt (action);
              CARD_Unmount (CARDSLOT);
              return 0;
            }
            CARD_Close (&CardFile);
            CARD_Delete(CARDSLOT, "TEMP");
          }

          // always delete existing slot
          CARD_Delete(CARDSLOT, filename);
        }

        // Create a new slot
        CardError = CARD_Create (CARDSLOT, filename, blocks, &CardFile);
        if (CardError)
        {
          sprintf (action, "Error create : %d %d", CardError, CARDSLOT);
          WaitPrompt (action);
          CARD_Unmount (CARDSLOT);
          return 0;
        }

      // Continue and save
      CARD_GetStatus(CARDSLOT, CardFile.filenum, &CardStatus);
      CardStatus.icon_addr = 0x0;
      CardStatus.icon_fmt = 2;
      CardStatus.icon_speed = 1;
      CardStatus.comment_addr = 2048;
      CARD_SetStatus(CARDSLOT, CardFile.filenum, &CardStatus);

      // And write the blocks out
      sbo = 0;
      while ( outbytes > 0 )
      {
          CardError = CARD_Write (&CardFile, &savebuffer[sbo], SectorSize, sbo);
          outbytes -= SectorSize;
          sbo += SectorSize;
      }    

      CARD_Close(&CardFile);
      CARD_Unmount(CARDSLOT);
        sprintf (action, "Saved %d bytes successfully", blocks);
        WaitPrompt (action);
        return 1;

    default: // Loading
      if (!CardFileExists (filename, CARDSLOT))
      {
        WaitPrompt ("No Save File Found");
        CARD_Unmount (CARDSLOT);
        return 0;
      }

      memset (&CardFile, 0, sizeof (CardFile));
      CardError = CARD_Open (CARDSLOT, filename, &CardFile);
      if (CardError)
      {
        sprintf (action, "Error Open : %d", CardError);
        WaitPrompt (action);
        CARD_Unmount (CARDSLOT);
        return 0;
      }

      blocks = CardFile.len;
      if (blocks < SectorSize) blocks = SectorSize;
      if (blocks % SectorSize) blocks++;

      // Just read the file back in
      sbo = 0;
      int size = blocks;
      while (blocks > 0)
      {
        CARD_Read (&CardFile, &savebuffer[sbo], SectorSize, sbo);
        sbo += SectorSize;
        blocks -= SectorSize;
      }
      CARD_Close(&CardFile);
      CARD_Unmount(CARDSLOT);
  
      memcpy(WRAM, &savebuffer[2112], 0x2000);
      ResetPCE();
      ResetSound();
      savetimer = 0;

      // Inform user
      sprintf (action, "Loaded %d bytes successfully", size);
      WaitPrompt (action);
    return 1;
    }
  }
  else WaitPrompt ("Unable to mount memory card");
  return 0; // Signal failure
}
