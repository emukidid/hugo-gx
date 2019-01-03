/****************************************************************************
 * Hu-Go! Nintendo Gamecube
 *
 * Sound Functions
 ****************************************************************************/
#include "pce.h"
#include "sys_snd.h"
#include "sound.h"
#include "osd_ngc_machine.h"
#include "osd_ngc_mix.h"

#define AUDIOBUFFER 2048

unsigned char SoundBuffer[2][AUDIOBUFFER] __attribute__ ((__aligned__ (32)));
int whichab = 0;

extern int gamepaused;
extern void ResetSound();

/****************************************************************************
 * This is the main callback routine.
 *
 * It is called whenever Audio DMA completes.
 ****************************************************************************/
static void PCEAudio ()
{
// AUDIO_StopDMA ();
  DCFlushRange (SoundBuffer[whichab], AUDIOBUFFER);
  whichab ^= 1;
  memset (SoundBuffer[whichab], 0, AUDIOBUFFER);
  AUDIO_InitDMA ((u32) SoundBuffer[whichab], AUDIOBUFFER);
  AUDIO_StartDMA ();

  if (!gamepaused)
    // Call the Hu-Go! Sound Generator
    MixStereoSound ((short *) SoundBuffer[whichab], AUDIOBUFFER >> 2);
}

/****************************************************************************
 * Sound is taken outside of the Hu-Go! SDL driver and is performed
 * entirely by ngc_mix.c
 *
 * This function kicks off the dma Audio system.
 ****************************************************************************/
int osd_snd_init_sound(void)
{
  // Required values for Nintendo Gamecube
  host.sound.stereo = 1;
  host.sound.sample_size = 512;
  host.sound.freq = 32000;
  host.sound.signed_sound = 1;

  // Start audio subsystem
  AUDIO_Init (NULL);

  memset (SoundBuffer[0], 0, AUDIOBUFFER << 1);

  // Set default samplerate to 32khz
  AUDIO_SetDSPSampleRate (AI_SAMPLERATE_32KHZ);

  // and the DMA Callback
  AUDIO_RegisterDMACallback (PCEAudio);
  DCFlushRange ((char *) SoundBuffer[0], AUDIOBUFFER);
  PCEAudio ();

  // initialize PSG channels
  InitPSG( host.sound.freq );

  return 1;
}

void osd_snd_trash_sound(void)
{
}

void osd_snd_set_volume(UChar v)
{
}

