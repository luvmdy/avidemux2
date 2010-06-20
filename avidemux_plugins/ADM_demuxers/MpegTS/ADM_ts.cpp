/***************************************************************************
    copyright            : (C) 2007/2009 by mean
    email                : fixounet@free.fr
    

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ADM_default.h"
#include "fourcc.h"
#include "DIA_coreToolkit.h"
#include "ADM_indexFile.h"
#include "ADM_ts.h"

#include <math.h>

uint32_t ADM_UsecFromFps1000(uint32_t fps1000);

/**
      \fn open
      \brief open the flv file, gather infos and build index(es).
*/

uint8_t tsHeader::open(const char *name)
{
    char *idxName=(char *)alloca(strlen(name)+4);
    bool r=false;
    FP_TYPE appendType=FP_DONT_APPEND;
    uint32_t append;
    char *type;
    uint64_t startDts;
    uint32_t version=0;

    sprintf(idxName,"%s.idx2",name);
    indexFile index;
    if(!index.open(idxName))
    {
        printf("[tsDemux] Cannot open index file %s\n",idxName);
        return false;
    }
    if(!index.readSection("System"))
    {
        printf("[tsDemux] Cannot read system section\n");
        goto abt;
    }
    type=index.getAsString("Type");
    if(!type || type[0]!='T')
    {
        printf("[tsDemux] Incorrect or not found type\n");
        goto abt;
    }

    version=index.getAsUint32("Version");
    if(version!=ADM_INDEX_FILE_VERSION)
    {
        GUI_Error_HIG("Error","This file's index has been created with an older version of avidemux.\nPlease delete the idx2 file and reopen.");
        goto abt;
    }
    append=index.getAsUint32("Append");
    printf("[tsDemux] Append=%"LU"\n",append);
    if(append) appendType=FP_APPEND;
    if(!parser.open(name,&appendType))
    {
        printf("[tsDemux] Cannot open root file (%s)\n",name);
        goto abt;
    }
    if(!readVideo(&index)) 
    {
        printf("[tsDemux] Cannot read Video section of %s\n",idxName);
        goto abt;
    }

    if(!readAudio(&index,name)) 
    {
        printf("[tsDemux] Cannot read Audio section of %s => No audio\n",idxName);
    }

    if(!readIndex(&index))
    {
        printf("[tsDemux] Cannot read index for file %s\n",idxName);
        goto abt;
    }
    updateIdr();
    updatePtsDts();
    _videostream.dwLength= _mainaviheader.dwTotalFrames=ListOfFrames.size();
    printf("[tsDemux] Found %d video frames\n",_videostream.dwLength);
    if(_videostream.dwLength)_isvideopresent=1;
//***********
    
    tsPacket=new tsPacketLinear(videoPid);
    if(tsPacket->open(name,appendType)==false) 
    {
        printf("tsDemux] Cannot tsPacket open the file\n");
        goto abt;
    }
    r=true;
    for(int i=0;i<listOfAudioTracks.size();i++)
    {
        ADM_tsTrackDescriptor *desc=listOfAudioTracks[i];
        ADM_audioStream *audioStream=ADM_audioCreateStream(&desc->header,desc->access);
        if(!audioStream)
        {
            
        }else       
        {
                desc->stream=audioStream;
        }
    }
abt:
    index.close();
    printf("[tsDemuxer] Loaded %d\n",r);
    return r;
}
/**
        \fn getVideoDuration
        \brief Returns duration of video in us
*/
uint64_t tsHeader::getVideoDuration(void)
{
    int index=ListOfFrames.size();
    if(!index) return 0;
    index--;
    int offset=0;
    do
    {
        if(ListOfFrames[index]->dts!=ADM_NO_PTS) break;
        index--;
        offset++;

    }while(index);
    if(!index)
    {
        ADM_error("Cannot find a valid DTS in the file\n");
        return 0;
    }
    float f,g;
    f=1000*1000*1000;
    f/=_videostream.dwRate; 
    g=ListOfFrames[index]->dts;
    g+=f*offset;
    return (uint64_t)g;
}
/**
    \fn updateIdr
    \brief if IDR are present, handle them as intra and I as P frame
            if not, handle I as intra, there might be some badly decoded frames (missing ref)
*/
bool tsHeader::updateIdr()
{
    int nbIdr=0;
    int nbI=0,nbP=0,nbB=0;
    if(!ListOfFrames.size()) return false;
    for(int i=0;i<ListOfFrames.size();i++)
    {
        int type=ListOfFrames[i]->type;
        switch(type)
        {
            case 1: nbI++;break;
            case 2: nbP++;break;
            case 3: nbB++;break;
            case 4: nbIdr++;break;
            default:
                    ADM_assert(0);
                    break;
        }
    }
    printf("[TsDemuxer] Found %d I, %d B, %d P\n",nbI,nbB,nbP);
    printf("[TsH264] Found %d IDR\n",nbIdr);
    if(nbIdr) // Change IDR to I and I to P...
    { 
        printf("[TsH264] Remapping IDR to I and I TO P\n");
        for(int i=0;i<ListOfFrames.size();i++)
        {
            switch(ListOfFrames[i]->type)
            {
                case 4: ListOfFrames[i]->type=1;break;
                case 1: 
                        if(i)
                            ListOfFrames[i]->type=2;break;
                default: break;
            }
        }
    }
    return true;
}
/**
    \fn getAudioInfo
    \brief returns wav header for stream i (=0)
*/
WAVHeader *tsHeader::getAudioInfo(uint32_t i )
{
        if(!listOfAudioTracks.size()) return NULL;
      ADM_assert(i<listOfAudioTracks.size());
      return listOfAudioTracks[i]->stream->getInfo();
      
}
/**
   \fn getAudioStream
*/

uint8_t   tsHeader::getAudioStream(uint32_t i,ADM_audioStream  **audio)
{
    if(!listOfAudioTracks.size())
    {
            *audio=NULL;
            return true;
    }
  ADM_assert(i<listOfAudioTracks.size());
  *audio=listOfAudioTracks[i]->stream;
  return true; 
}
/**
    \fn getNbAudioStreams

*/
uint8_t   tsHeader::getNbAudioStreams(void)
{
 
  return listOfAudioTracks.size(); 
}
/*
    __________________________________________________________
*/

void tsHeader::Dump(void)
{
 
}
/**
    \fn close
    \brief cleanup
*/

uint8_t tsHeader::close(void)
{
    ADM_info("Destroying TS demuxer\n");
    // Destroy index
    int n=ListOfFrames.size();
    for(int i=0;i<n;i++)
        delete ListOfFrames[i];
    ListOfFrames.clear();

    // Destroy audio tracks
    n=listOfAudioTracks.size();
    for(int i=0;i<n;i++)
    {
        ADM_tsTrackDescriptor *desc=listOfAudioTracks[i];
        delete desc;
        listOfAudioTracks[i]=NULL;
    } // Container will be destroyed by vector destructor
    listOfAudioTracks.clear();
    if(tsPacket)
    {
        tsPacket->close();
        delete tsPacket;
        tsPacket=NULL;
    }
    return 1;
}
/**
    \fn tsHeader
    \brief constructor
*/

 tsHeader::tsHeader( void ) : vidHeader()
{ 
    interlaced=false;
    lastFrame=0xffffffff;
    videoPid=0;
    
}
/**
    \fn tsHeader
    \brief destructor
*/

 tsHeader::~tsHeader(  )
{
  close();
}


/**
    \fn setFlag
    \brief Returns timestamp in us of frame "frame" (PTS)
*/

uint8_t  tsHeader::setFlag(uint32_t frame,uint32_t flags)
{
    if(frame>=ListOfFrames.size()) return 0;
     uint32_t f=2;
     uint32_t intra=flags & AVI_FRAME_TYPE_MASK;
     if(intra & AVI_KEY_FRAME) f=1;
     if(intra & AVI_B_FRAME) f=3;
     
      ListOfFrames[frame]->type=f;
      ListOfFrames[frame]->pictureType=flags & AVI_STRUCTURE_TYPE_MASK;
    return 1;
}
/**
    \fn getFlags
    \brief Returns timestamp in us of frame "frame" (PTS)
*/

uint32_t tsHeader::getFlags(uint32_t frame,uint32_t *flags)
{
    if(frame>=ListOfFrames.size()) return 0;
    uint32_t f=ListOfFrames[frame]->type;
    switch(f)
    {
        case 1: *flags=AVI_KEY_FRAME;break;
        case 2: *flags=0;break;
        case 3: *flags=AVI_B_FRAME;break;
    }
    *flags=*flags+ListOfFrames[frame]->pictureType;
    return  1;
}

/**
    \fn getTime
    \brief Returns timestamp in us of frame "frame" (PTS)
*/
uint64_t tsHeader::getTime(uint32_t frame)
{
   if(frame>=ListOfFrames.size()) return 0;
    uint64_t pts=ListOfFrames[frame]->pts;
    return pts;
}
/**
    \fn timeConvert
    \brief FIXME
*/
uint64_t tsHeader::timeConvert(uint64_t x)
{
    if(x==ADM_NO_PTS) return ADM_NO_PTS;
    x=x-ListOfFrames[0]->dts;
    x=x*1000;
    x/=90;
    return x;

}
/**
        \fn getFrame
*/

uint8_t  tsHeader::getFrame(uint32_t frame,ADMCompressedImage *img)
{
    if(frame>=ListOfFrames.size()) return 0;
    dmxFrame *pk=ListOfFrames[frame];
    // next frame
    if(frame==(lastFrame+1) && pk->type!=1)
    {
        lastFrame++;
        bool r=tsPacket->read(pk->len,img->data);
             img->dataLength=pk->len;
             img->demuxerFrameNo=frame;
             img->demuxerDts=pk->dts;
             img->demuxerPts=pk->pts;
             //printf("[>>>] %d:%02x %02x %02x %02x\n",frame,img->data[0],img->data[1],img->data[2],img->data[3]);
             getFlags(frame,&(img->flags));
             return r;
    }
    // Intra ?
    if(pk->type==1)
    {
        if(!tsPacket->seek(pk->startAt,pk->index)) return false;
         bool r=tsPacket->read(pk->len,img->data);
             img->dataLength=pk->len;
             img->demuxerFrameNo=frame;
             img->demuxerDts=pk->dts;
             img->demuxerPts=pk->pts;
             getFlags(frame,&(img->flags));
             //printf("[>>>] %d:%02x %02x %02x %02x\n",frame,img->data[0],img->data[1],img->data[2],img->data[3]);
             lastFrame=frame;
             return r;

    }
    
    // Random frame
    // Need to rewind, then forward
    int startPoint=frame;
    while(startPoint && !ListOfFrames[startPoint]->startAt) startPoint--;
    printf("[tsDemux] Wanted frame %"LU", going back to frame %"LU", last frame was %"LU",\n",frame,startPoint,lastFrame);
    pk=ListOfFrames[startPoint];
    if(!tsPacket->seek(pk->startAt,pk->index)) 
    {
            printf("[tsDemux] Failed to rewind to frame %"LU"\n",startPoint);
            return false;
    }
    // Now forward
    while(startPoint<frame)
    {
        pk=ListOfFrames[startPoint];
        if(!tsPacket->read(pk->len,img->data))
        {
            printf("[tsDemux] Read fail for frame %"LU"\n",startPoint);
            lastFrame=0xffffffff;
            return false;
        }
        startPoint++;
        lastFrame=startPoint;
    }
    pk=ListOfFrames[frame];
    lastFrame++;
    bool r=tsPacket->read(pk->len,img->data);
         img->dataLength=pk->len;
         img->demuxerFrameNo=frame;
         img->demuxerDts=pk->dts;
         img->demuxerPts=pk->pts;
         //printf("[>>>] %d:%02x %02x %02x %02x\n",frame,img->data[0],img->data[1],img->data[2],img->data[3]);
         getFlags(frame,&(img->flags));
         return r;
}
/**
        \fn getExtraHeaderData
*/
uint8_t  tsHeader::getExtraHeaderData(uint32_t *len, uint8_t **data)
{
                *len=0; //_tracks[0].extraDataLen;
                *data=NULL; //_tracks[0].extraData;
                return true;            
}

/**
      \fn getFrameSize
      \brief return the size of frame frame
*/
uint8_t tsHeader::getFrameSize (uint32_t frame, uint32_t * size)
{
    if(frame>=ListOfFrames.size()) return 0;
    *size=ListOfFrames[frame]->len;
    return true;
}

/**
    \fn getPtsDts
*/
bool    tsHeader::getPtsDts(uint32_t frame,uint64_t *pts,uint64_t *dts)
{
    if(frame>=ListOfFrames.size()) return false;
    dmxFrame *pk=ListOfFrames[frame];

    *dts=pk->dts;
    *pts=pk->pts;
    return true;
}
/**
        \fn setPtsDts
*/
bool    tsHeader::setPtsDts(uint32_t frame,uint64_t pts,uint64_t dts)
{
      if(frame>=ListOfFrames.size()) return false;
    dmxFrame *pk=ListOfFrames[frame];

    pk->dts=dts;
    pk->pts=pts;
    return true;


}


//EOF
