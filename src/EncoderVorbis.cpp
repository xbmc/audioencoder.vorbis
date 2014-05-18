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
#include "xbmc/xbmc_audioenc_dll.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

extern "C" {

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
    if (strcmp((const char*)value,"0") == 0)
      preset = 4;
    if (strcmp((const char*)value,"1") == 0)
      preset = 5;
    if (strcmp((const char*)value,"2") == 0)
      preset = 7;
  }
  if (strcmp(strSetting,"bitrate") == 0)
    bitrate = 128+32*atoi((const char*)value);

  return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

vorbis_info m_sVorbisInfo; /* struct that stores all the static vorbis bitstream settings */
vorbis_dsp_state m_sVorbisDspState; /* central working state for the packet->PCM decoder */
vorbis_block m_sVorbisBlock; /* local working space for packet->PCM decode */
vorbis_comment m_sVorbisComment;

ogg_stream_state m_sOggStreamState; /* take physical pages, weld into a logical stream of packets */
ogg_page m_sOggPage; /* one Ogg bitstream page.  Vorbis packets are inside */
ogg_packet m_sOggPacket; /* one raw packet of data for decode */

uint8_t* m_pBuffer;
bool first;

bool Init(int iInChannels, int iInRate, int iInBits,
          const char* title, const char* artist,
          const char* albumartist, const char* album,
          const char* year, const char* track, const char* genre,
          const char* comment, int iTrackLength)
{
  // we only accept 2 / 44100 / 16 atm
  if (iInChannels != 2 || iInRate != 44100 || iInBits != 16) return false;

  vorbis_info_init(&m_sVorbisInfo);
  if (preset == -1)
    vorbis_encode_init(&m_sVorbisInfo, iInChannels, iInRate, -1, bitrate*1000, -1);
  else
    vorbis_encode_init_vbr(&m_sVorbisInfo, iInChannels, iInRate, double(preset)/10.0);

  /* add a comment */
  vorbis_comment_init(&m_sVorbisComment);
  vorbis_comment_add_tag(&m_sVorbisComment, (char*)"comment", (char*)comment);
  vorbis_comment_add_tag(&m_sVorbisComment, (char*)"artist", (char*)artist);
  vorbis_comment_add_tag(&m_sVorbisComment, (char*)"title", (char*)title);
  vorbis_comment_add_tag(&m_sVorbisComment, (char*)"album", (char*)album);
  vorbis_comment_add_tag(&m_sVorbisComment, (char*)"albumartist", (char*)albumartist);
  vorbis_comment_add_tag(&m_sVorbisComment, (char*)"genre", (char*)genre);
  vorbis_comment_add_tag(&m_sVorbisComment, (char*)"tracknumber", (char*)track);
  vorbis_comment_add_tag(&m_sVorbisComment, (char*)"date", (char*)year);

  /* set up the analysis state and auxiliary encoding storage */
  vorbis_analysis_init(&m_sVorbisDspState, &m_sVorbisInfo);

  vorbis_block_init(&m_sVorbisDspState, &m_sVorbisBlock);

  /* set up our packet->stream encoder */
  /* pick a random serial number; that way we can more likely build
  chained streams just by concatenation */
  srand(time(NULL));
  ogg_stream_init(&m_sOggStreamState, rand());

  m_pBuffer = new uint8_t[4096];

  first = true;

  return true;
}

int Encode(int nNumBytesRead, uint8_t* pbtStream, uint8_t* buffer)
{
  uint8_t* sbuffer = buffer;
  if (first)
  {
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_analysis_headerout(&m_sVorbisDspState, &m_sVorbisComment,
                              &header, &header_comm, &header_code);

    ogg_stream_packetin(&m_sOggStreamState, &header);
    ogg_stream_packetin(&m_sOggStreamState, &header_comm);
    ogg_stream_packetin(&m_sOggStreamState, &header_code);

    /* This ensures the actual
    * audio data will start on a new page, as per spec
    */
    while (1)
    {
      int result = ogg_stream_flush(&m_sOggStreamState, &m_sOggPage);
      if (result == 0)
        break;
      memcpy(buffer, m_sOggPage.header, m_sOggPage.header_len);
      buffer += m_sOggPage.header_len;
      memcpy(buffer, m_sOggPage.body, m_sOggPage.body_len);
      buffer += m_sOggPage.body_len;
    }
    first = false;
  }
  int eos = 0;

  /* data to encode */
  int nBlocks = (int)(nNumBytesRead / 4096);
  int nBytesleft = nNumBytesRead - nBlocks * 4096;
  int block = 4096;

  for (int a = 0; a <= nBlocks; a++)
  {
    if (a == nBlocks)
    {
      // no more blocks of 4096 bytes to write, just write the last bytes
      block = nBytesleft;
    }

    /* expose the buffer to submit data */
    float **buffer2 = vorbis_analysis_buffer(&m_sVorbisDspState, 1024);

    /* uninterleave samples */
    memcpy(m_pBuffer, pbtStream, block);
    pbtStream += 4096;
    int iSamples = block / (2 * 2);
    signed char* buf = (signed char*) m_pBuffer;
    for (int i = 0; i < iSamples; i++)
    {
      int j = i << 2; // j = i * 4
      buffer2[0][i] = (((long)buf[j + 1] << 8) | (0x00ff & (int)buf[j])) / 32768.0f;
      buffer2[1][i] = (((long)buf[j + 3] << 8) | (0x00ff & (int)buf[j + 2])) / 32768.0f;
    }

    /* tell the library how much we actually submitted */
    vorbis_analysis_wrote(&m_sVorbisDspState, iSamples);

    /* vorbis does some data preanalysis, then divvies up blocks for
    more involved (potentially parallel) processing.  Get a single
    block for encoding now */
    while (vorbis_analysis_blockout(&m_sVorbisDspState, &m_sVorbisBlock) == 1)
    {
      /* analysis, assume we want to use bitrate management */
      vorbis_analysis(&m_sVorbisBlock, NULL);
      vorbis_bitrate_addblock(&m_sVorbisBlock);

      while (vorbis_bitrate_flushpacket(&m_sVorbisDspState, &m_sOggPacket))
      {
        /* weld the packet into the bitstream */
        ogg_stream_packetin(&m_sOggStreamState, &m_sOggPacket);

        /* write out pages (if any) */
        while (!eos)
        {
          int result = ogg_stream_pageout(&m_sOggStreamState, &m_sOggPage);
          if (result == 0)
            break;
          memcpy(buffer, m_sOggPage.header, m_sOggPage.header_len);
          buffer += m_sOggPage.header_len;
          memcpy(buffer, m_sOggPage.body, m_sOggPage.body_len);
          buffer += m_sOggPage.body_len;

          /* this could be set above, but for illustrative purposes, I do
          it here (to show that vorbis does know where the stream ends) */
          if (ogg_page_eos(&m_sOggPage)) eos = 1;
        }
      }
    }
  }

  return buffer-sbuffer;
}

int Flush(uint8_t* buffer)
{
  int eos = 0;
  // tell vorbis we are encoding the end of the stream
  vorbis_analysis_wrote(&m_sVorbisDspState, 0);
  uint8_t* sbuffer = buffer;
  while (vorbis_analysis_blockout(&m_sVorbisDspState, &m_sVorbisBlock) == 1)
  {
    /* analysis, assume we want to use bitrate management */
    vorbis_analysis(&m_sVorbisBlock, NULL);
    vorbis_bitrate_addblock(&m_sVorbisBlock);

    while (vorbis_bitrate_flushpacket(&m_sVorbisDspState, &m_sOggPacket))
    {
      /* weld the packet into the bitstream */
      ogg_stream_packetin(&m_sOggStreamState, &m_sOggPacket);

      /* write out pages (if any) */
      while (!eos)
      {
        int result = ogg_stream_pageout(&m_sOggStreamState, &m_sOggPage);
        if (result == 0)
          break;
        memcpy(buffer, m_sOggPage.header, m_sOggPage.header_len);
        buffer += m_sOggPage.header_len;
        memcpy(buffer, m_sOggPage.body, m_sOggPage.body_len);
        buffer += m_sOggPage.body_len;

        /* this could be set above, but for illustrative purposes, I do
        it here (to show that vorbis does know where the stream ends) */
        if (ogg_page_eos(&m_sOggPage)) eos = 1;
      }
    }
  }

  return buffer-sbuffer;
}

bool Close(const char* File)
{
  /* clean up and exit.  vorbis_info_clear() must be called last */
  ogg_stream_clear(&m_sOggStreamState);
  vorbis_block_clear(&m_sVorbisBlock);
  vorbis_dsp_clear(&m_sVorbisDspState);
  vorbis_comment_clear(&m_sVorbisComment);
  vorbis_info_clear(&m_sVorbisInfo);

  /* ogg_page and ogg_packet structs always point to storage in
     libvorbis.  They're never freed or manipulated directly */
  delete []m_pBuffer;
  m_pBuffer = NULL;

  return true;
}

}
