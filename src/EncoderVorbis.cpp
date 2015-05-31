/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>
#include "xbmc_audioenc_dll.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>

extern "C" {

// settings (global)
int preset=-1;
int bitrate=0;

//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  return ADDON_STATUS_NEED_SETTINGS;
}

//-- Stop ---------------------------------------------------------------------
// This dll must cease all runtime activities
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Stop()
{
}

//-- Destroy ------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Destroy()
{
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
bool ADDON_HasSettings()
{
  return true;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_GetStatus()
{
  return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------

void ADDON_FreeSettings()
{
}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_SetSetting(const char *strSetting, const void* value)
{
  if (strcmp(strSetting,"preset") == 0)
  {
    int ival = *((int*)value);
    if (ival == 0)
      preset = 4;
    else if (ival == 1)
      preset = 5;
    else if (ival == 2)
      preset = 7;
  }
  if (strcmp(strSetting,"bitrate") == 0)
  {
    int ival = *((int*)value);
    bitrate = 128 + 32 * ival;
  }

  return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

static const int OGG_BLOCK_FRAMES = 1024; // number of frames to encode at a time

class ogg_context
{
public:
  ogg_context(audioenc_callbacks &cb, vorbis_info &info) :
    callbacks(cb),
    vorbisInfo(info),
    inited(false)
  {
  };

  audioenc_callbacks callbacks;              ///< callbacks to write/seek etc.

  vorbis_info        vorbisInfo;             ///< struct that stores all the static vorbis bitstream settings
  vorbis_dsp_state   vorbisDspState;         ///< central working state for the packet->PCM decoder
  vorbis_block       vorbisBlock;            ///< local working space for packet->PCM decode

  ogg_stream_state   oggStreamState;         ///< take physical pages, weld into a logical stream of packets

  bool               inited;                 ///< whether Init() was successful
};

void *Create(audioenc_callbacks *callbacks)
{
  if (callbacks && callbacks->write && callbacks->seek)
  {
    // create encoder context
    vorbis_info info;
    vorbis_info_init(&info);

    return new ogg_context(*callbacks, info);
  }
  return NULL;
}

bool Start(void *ctx, int iInChannels, int iInRate, int iInBits,
          const char* title, const char* artist,
          const char* albumartist, const char* album,
          const char* year, const char* track, const char* genre,
          const char* comment, int iTrackLength)
{
  ogg_context *context = (ogg_context *)ctx;
  if (!context || !context->callbacks.write)
    return false;

  // we accept only 2 ch 16bit atm
  if (iInChannels != 2 || iInBits != 16)
    return false;

  if (preset == -1)
    vorbis_encode_init(&context->vorbisInfo, iInChannels, iInRate, -1, bitrate*1000, -1);
  else
    vorbis_encode_init_vbr(&context->vorbisInfo, iInChannels, iInRate, float(preset)/10.0f);

  /* add a comment */
  vorbis_comment comm;
  vorbis_comment_init(&comm);
  vorbis_comment_add_tag(&comm, (char*)"comment", (char*)comment);
  vorbis_comment_add_tag(&comm, (char*)"artist", (char*)artist);
  vorbis_comment_add_tag(&comm, (char*)"title", (char*)title);
  vorbis_comment_add_tag(&comm, (char*)"album", (char*)album);
  vorbis_comment_add_tag(&comm, (char*)"albumartist", (char*)albumartist);
  vorbis_comment_add_tag(&comm, (char*)"genre", (char*)genre);
  vorbis_comment_add_tag(&comm, (char*)"tracknumber", (char*)track);
  vorbis_comment_add_tag(&comm, (char*)"date", (char*)year);

  /* set up the analysis state and auxiliary encoding storage */
  vorbis_analysis_init(&context->vorbisDspState, &context->vorbisInfo);

  vorbis_block_init(&context->vorbisDspState, &context->vorbisBlock);

  /* set up our packet->stream encoder */
  /* pick a random serial number; that way we can more likely build
  chained streams just by concatenation */
  srand((unsigned int)time(NULL));
  ogg_stream_init(&context->oggStreamState, rand());

  /* write out the metadata */
  ogg_packet header;
  ogg_packet header_comm;
  ogg_packet header_code;
  ogg_page   page;

  vorbis_analysis_headerout(&context->vorbisDspState, &comm,
                            &header, &header_comm, &header_code);

  ogg_stream_packetin(&context->oggStreamState, &header);
  ogg_stream_packetin(&context->oggStreamState, &header_comm);
  ogg_stream_packetin(&context->oggStreamState, &header_code);

  while (1)
  {
    /* This ensures the actual
     * audio data will start on a new page, as per spec
     */
    int result = ogg_stream_flush(&context->oggStreamState, &page);
    if (result == 0)
      break;
    context->callbacks.write(context->callbacks.opaque, page.header, page.header_len);
    context->callbacks.write(context->callbacks.opaque, page.body, page.body_len);
  }
  vorbis_comment_clear(&comm);

  context->inited = true;
  return true;
}

int Encode(void *ctx, int nNumBytesRead, uint8_t* pbtStream)
{
  ogg_context *context = (ogg_context *)ctx;
  if (!context || !context->callbacks.write)
    return -1;

  int eos = 0;

  int bytes_left = nNumBytesRead;
  while (bytes_left)
  {
    const int channels = 2;
    const int bits_per_channel = 16;

    float **buffer = vorbis_analysis_buffer(&context->vorbisDspState, OGG_BLOCK_FRAMES);

    /* uninterleave samples */

    int bytes_per_frame = channels * (bits_per_channel >> 3);
    int frames = std::min(bytes_left / bytes_per_frame, OGG_BLOCK_FRAMES);

    int16_t* buf = (int16_t*)pbtStream;
    for (int i = 0; i < frames; i++)
    {
      for (int j = 0; j < channels; j++)
        buffer[j][i] = (*buf++) / 32768.0f;
    }
    pbtStream  += frames * bytes_per_frame;
    bytes_left -= frames * bytes_per_frame;

    /* tell the library how much we actually submitted */
    vorbis_analysis_wrote(&context->vorbisDspState, frames);

    /* vorbis does some data preanalysis, then divvies up blocks for
    more involved (potentially parallel) processing.  Get a single
    block for encoding now */
    while (vorbis_analysis_blockout(&context->vorbisDspState, &context->vorbisBlock) == 1)
    {
      /* analysis, assume we want to use bitrate management */
      vorbis_analysis(&context->vorbisBlock, NULL);
      vorbis_bitrate_addblock(&context->vorbisBlock);

      ogg_packet packet;
      ogg_page   page;
      while (vorbis_bitrate_flushpacket(&context->vorbisDspState, &packet))
      {
        /* weld the packet into the bitstream */
        ogg_stream_packetin(&context->oggStreamState, &packet);

        /* write out pages (if any) */
        while (!eos)
        {
          int result = ogg_stream_pageout(&context->oggStreamState, &page);
          if (result == 0)
            break;
          context->callbacks.write(context->callbacks.opaque, page.header, page.header_len);
          context->callbacks.write(context->callbacks.opaque, page.body, page.body_len);

          /* this could be set above, but for illustrative purposes, I do
          it here (to show that vorbis does know where the stream ends) */
          if (ogg_page_eos(&page)) eos = 1;
        }
      }
    }
  }

  // return bytes consumed
  return nNumBytesRead - bytes_left;
}

bool Finish(void *ctx)
{
  ogg_context *context = (ogg_context *)ctx;
  if (!context || !context->callbacks.write)
    return false;

  int eos = 0;
  // tell vorbis we are encoding the end of the stream
  vorbis_analysis_wrote(&context->vorbisDspState, 0);
  while (vorbis_analysis_blockout(&context->vorbisDspState, &context->vorbisBlock) == 1)
  {
    /* analysis, assume we want to use bitrate management */
    vorbis_analysis(&context->vorbisBlock, NULL);
    vorbis_bitrate_addblock(&context->vorbisBlock);

    ogg_packet packet;
    ogg_page   page;
    while (vorbis_bitrate_flushpacket(&context->vorbisDspState, &packet))
    {
      /* weld the packet into the bitstream */
      ogg_stream_packetin(&context->oggStreamState, &packet);

      /* write out pages (if any) */
      while (!eos)
      {
        int result = ogg_stream_pageout(&context->oggStreamState, &page);
        if (result == 0)
          break;
        context->callbacks.write(context->callbacks.opaque, page.header, page.header_len);
        context->callbacks.write(context->callbacks.opaque, page.body, page.body_len);

        /* this could be set above, but for illustrative purposes, I do
        it here (to show that vorbis does know where the stream ends) */
        if (ogg_page_eos(&page)) eos = 1;
      }
    }
  }
  return true;
}

void Free(void *ctx)
{
  ogg_context *context = (ogg_context *)ctx;
  if (!context)
    return;

  /* clean up and exit.  vorbis_info_clear() must be called last */
  if (context->inited)
  {
    ogg_stream_clear(&context->oggStreamState);
    vorbis_block_clear(&context->vorbisBlock);
    vorbis_dsp_clear(&context->vorbisDspState);
  }
  vorbis_info_clear(&context->vorbisInfo);

  return;
}

}
