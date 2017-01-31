/***************************************************************************
    \brief TS indexer, H265 video
    \author mean fixounet@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "ADM_tsIndex.h"
#include "DIA_coreToolkit.h"
#include "ADM_tsIndex.h"

#define LIST_OF_NAL_TYPE\
    NAME(NAL_H265_TRAIL_N,      0)    , \
    NAME(NAL_H265_TRAIL_R,      1)    , \
    NAME(NAL_H265_TSA_N      , 2),\
    NAME(NAL_H265_TSA_R      , 3),\
    NAME(NAL_H265_STSA_N     , 4),\
    NAME(NAL_H265_STSA_R     , 5),\
    NAME(NAL_H265_RADL_N     , 6),\
    NAME(NAL_H265_RADL_R     , 7),\
    NAME(NAL_H265_RASL_N     , 8),\
    NAME(NAL_H265_RASL_R     , 9),\
    NAME(NAL_H265_BLA_W_LP   , 16),\
    NAME(NAL_H265_BLA_W_RADL , 17),\
    NAME(NAL_H265_BLA_N_LP   , 18),\
    NAME(NAL_H265_IDR_W_RADL , 19),\
    NAME(NAL_H265_IDR_N_LP,     20) , \
    NAME(NAL_H265_CRA_NUT    ,  21),\
    NAME(NAL_H265_VPS  ,        32)    ,\
    NAME(NAL_H265_SPS  ,        33)    ,\
    NAME(NAL_H265_PPS  ,        34)    ,\
    NAME(NAL_H265_AUD  ,        35)    ,\
    NAME(NAL_H265_FD_NUT  ,        38)    ,\
    NAME(NAL_H265_SEI_PREFIX,   39),\
    NAME(NAL_H265_SEI_SUFFIX,   40),\


#define NAME(x,y) x= y

enum{
LIST_OF_NAL_TYPE
};
#undef NAME
#define NAME(x,y) {y,#x}

typedef struct NAL_DESC{int value; const char *name;}NAL_DESC;

NAL_DESC nalDesc[]={
    LIST_OF_NAL_TYPE
};
        
/**
        \fn decodeSEI
        \brief decode SEI to get short ref I
        @param recoveryLength # of recovery frame
        \return true if recovery found
*/
bool TsIndexer::decodeSEIH265(uint32_t nalSize, uint8_t *org,uint32_t *recoveryLength,
                pictureStructure *picStruct)
{
#if 0
    if(nalSize+16>=ADM_NAL_BUFFER_SIZE)
    {
        ADM_warning("SEI size too big, probably corrupted input (%u bytes)\n",nalSize);
        return false;
    }
    uint8_t *payload=payloadBuffer;
    bool r=false;
    nalSize=ADM_unescapeH264(nalSize,org,payload);
    uint8_t *tail=payload+nalSize;
    *picStruct=pictureFrame; // frame
    while( payload<tail-2)
    {
                uint32_t sei_type=0,sei_size=0;
                while(payload[0]==0xff) {sei_type+=0xff;payload++;};
                sei_type+=payload[0];payload++;
                while(payload[0]==0xff) {sei_size+=0xff;payload++;};
                sei_size+=payload[0];payload++;
                aprintf("  [SEI] Type: 0x%x, size: %u\n",sei_type,sei_size);
                if(payload+sei_size>=tail)
                {
                        return false;
                }
                switch(sei_type) // Recovery point
                {

                       case 1:
                        {
                            decoderSei1(spsInfo,sei_size,payload,picStruct);
                            payload+=sei_size;
                            break;
                        }
                       case 6:
                        {
                            decoderSei6(sei_size,payload,recoveryLength);
                            payload+=sei_size;
                            aprintf("[SEI] Recovery :%" PRIu32"\n",*recoveryLength);
                            r=true;
                            break;
                        }
                        default:
                            payload+=sei_size;
                            break;
                }
    }
    //if(payload+1<tail) ADM_warning("Bytes left in SEI %d\n",(int)(tail-payload));
    return r;
#endif
   return false;
}

/**
 * \fn findGivenStartCode
 * @param pkt
 * @param match
 * @return 
 */
static bool findGivenStartCode(tsPacketLinearTracker *pkt,int match, const char *name)
{
    bool keepRunning=true;    
    while(keepRunning)
    {
      int startCode=pkt->findStartCode();
      if(!pkt->stillOk())
      {
          return false;
      }     
      printf("Match %x %d\n",startCode,((startCode>>1)&0x3f));
      startCode=((startCode>>1)&0x3f);  
      
      if(startCode!=match && match) 
          continue;
        dmxPacketInfo packetInfo;
        pkt->getInfo( &packetInfo);
      
      ADM_info("%s found at 0x%x+0x%x\n",name,(int)packetInfo.startAt,packetInfo.offset);
      return true;
    }
    return false;
}

/**
 * \fn findGivenStartCode
 * @param pkt
 * @param match: Startcode to find, zero means any startcode
 * @return 
 */
static uint8_t * findGivenStartCodeInBuffer(uint8_t *start, uint8_t *end,int match, const char *name)
{
    
    while(start+4<end)
    {
        if(!start[0]&&!start[1] && start[2]==0x01)
        {
            uint8_t code=(start[3]>>1)&0x3f;
            printf(" Matcho = %d\n",code);
            if(code==match || !match) return start;
        }
        start++;
    }
    ADM_warning("Cannot find %s\n",name);
    return NULL;
}

/**
 * \fn findH264SPS
 * @return 
 */
bool TsIndexer::findH265VPS(tsPacketLinearTracker *pkt,TSVideo &video)
{    
    bool keepRunning=true;
    dmxPacketInfo packetInfo;
    uint8_t headerBuffer[512]={0,0,0,1,(NAL_H265_VPS<<1)}; // we are forcing some bits to be zero...
    // This is a bit naive...
    if(!findGivenStartCode(pkt,NAL_H265_VPS ,"VPS"))
    {
        ADM_warning("Cannot find HEVC VPS\n");
        return false;
    }   
    pkt->getInfo( &packetInfo);
    uint64_t startExtraData=packetInfo.startAt-193; // /!\ It may be in the previous packet, very unlikely though    
    pkt->read(512,headerBuffer+5);
    uint8_t *pointer=headerBuffer+5;
    uint8_t *end=headerBuffer+512;
    
    pointer=findGivenStartCodeInBuffer(pointer,end,NAL_H265_SPS,"SPS");
    if(!pointer)
    {
        ADM_warning("Cannot find HEVC SPS\n");
        return false;
    }
    ADM_info("SPS found at %d\n",(int)(pointer-headerBuffer));
    pointer=findGivenStartCodeInBuffer(pointer,end,NAL_H265_PPS,"PPS");
    if(!pointer)
    {
        ADM_warning("Cannot find HEVC PPS\n");
        return false;
    }
    ADM_info("PPS found at %d\n",(int)(pointer-headerBuffer));
    pointer=findGivenStartCodeInBuffer(pointer+3,end,0,"Any");
    if(!pointer)
    {
        ADM_warning("Cannot find HEVC next marker\n");
        return false;
    }
    ADM_info("Any found at %d\n",(int)(pointer-headerBuffer));
    int extraLen=(int)(pointer-headerBuffer); // should be enough (tm)    
    
    ADM_info("VPS/SPS/PPS lengths = %d bytes \n",extraLen);
    
    if(!extractSPSInfoH265(headerBuffer,extraLen,&info))
    {
        ADM_warning("Cannot extract SPS/VPS/PPS\n");
        return false;
    }
    video.w=info.width;
    video.h=info.height;
    ADM_info("Found video %d x %d\n",info.width,info.height);
    return true;
    
  
}
/**
 * \fn decodePictureType
 */
int  TsIndexer::decodePictureTypeH265(int nalType,getBits &bits)
{
    int slice=2;
    bool firstPic=bits.get(1);
    bool idr=false;
    if(nalType  >= 16 && nalType <= 23)
    {
        idr=true;
        bits.get(1); 
    }
    bits.getUEG(); // PPS
    if(!firstPic)
    {
        printf("*oops\n");
    }
    bits.skip(info.num_extra_slice_header_bits);
    int sliceType=bits.getUEG();
    switch(sliceType)
    {
        case 0: slice=3;break; // B
        case 1: slice=2;break; // P
        case 2: slice=2;        // I
                if(idr)
                        slice=4;
                break; // I        
    default:
                ADM_warning("Unknown slice type\n");
                break;
    }
    return slice;
}
/**
    \fn runH264
    \brief Index H264 stream
*/
bool TsIndexer::runH265(const char *file,ADM_TS_TRACK *videoTrac)
{

bool    seq_found=false;
bool    firstSps=true;
TS_PESpacket SEI_nal(0);
TSVideo video;
indexerData  data;

bool result=false;
bool bAppend=false;

    beginConsuming=0;
    listOfUnits.clear();

    printf("Starting H264 indexer\n");
    if(!videoTrac) return false;
    if(videoTrac[0].trackType!=ADM_TS_H265 
       )
    {
        printf("[Ts Indexer] Only H265 video supported\n");
        return false;
    }
    video.pid=videoTrac[0].trackPid;

    memset(&data,0,sizeof(data));
    data.picStructure=pictureFrame;
    string indexName=string(file);
    indexName=indexName+string(".idx2");
    index=qfopen(indexName,(const char*)"wt");

    if(!index)
    {
        printf("[PsIndex] Cannot create %s\n",indexName.c_str());
        return false;
    }


    pkt=new tsPacketLinearTracker(videoTrac->trackPid, audioTracks);

    FP_TYPE append=FP_DONT_APPEND;
    if(true==ADM_probeSequencedFile(file))
    {
        if(true==GUI_Question(QT_TRANSLATE_NOOP("tsdemuxer","There are several files with sequential file names. Should they be all loaded ?")))
                bAppend=true;
    }
    if(bAppend==true)
        append=FP_APPEND;
    writeSystem(file,bAppend);
    pkt->open(file,append);
    data.pkt=pkt;
    fullSize=pkt->getSize();
    gui=createProcessing(QT_TRANSLATE_NOOP("tsdemuxer","Indexing"),pkt->getSize());
    int lastRefIdc=0;
    bool keepRunning=true;
    //******************
    // 1 search SPS
    //******************
    switch(videoTrac[0].trackType)
    {
        case ADM_TS_H265 :
            seq_found=findH265VPS(pkt,video);
            break;
        default:
            break;
    }    
    if(!seq_found) goto the_end;

    video.w=1980*2;
    video.h=1080*2;
    video.fps=23976;
    writeVideo(&video,ADM_TS_H265);
    writeAudio();
    qfprintf(index,"[Data]\n");

    
     decodingImage=false;
    //******************
    // 2 Index
    //******************
        bool fourBytes;
      while(keepRunning)
      {
          fourBytes=false;
        int startCode=pkt->findStartCode2(fourBytes);
resume:
        if(!pkt->stillOk()) break;

        int startCodeLength=4;
        if(fourBytes==true) startCodeLength++;

        startCode=((startCode>>1)&0x3f);   
        printf("Startcode =%d\n",startCode);
          // Ignore multiple chunk of the same pic
          if((startCode==NAL_NON_IDR || startCode==NAL_IDR)&&decodingImage )
          {
            aprintf("Still capturing, ignore\n");
            continue;
          }
#define NON_IDR_PRE_READ 8 // FIXME

          switch(startCode)
                  {
                  case NAL_H265_AUD:
                        {
                          aprintf("AU DELIMITER\n");
                          decodingImage = false;
                        }
                          break;
                case NAL_H265_TRAIL_R:
                case NAL_H265_TRAIL_N:
                case NAL_H265_TSA_N:
                case NAL_H265_TSA_R:
                case NAL_H265_STSA_N:
                case NAL_H265_STSA_R:
                case NAL_H265_BLA_W_LP:
                case NAL_H265_BLA_W_RADL:
                case NAL_H265_BLA_N_LP:
                case NAL_H265_IDR_W_RADL:
                case NAL_H265_IDR_N_LP:
                case NAL_H265_CRA_NUT:
                case NAL_H265_RADL_N:
                case NAL_H265_RADL_R:
                case NAL_H265_RASL_N:
                case NAL_H265_RASL_R:
                {
                    uint8_t buffer[32],header[32];
                    int preRead=8;
                        data.nbPics++;
                        decodingImage=true;
                        pkt->getInfo(&thisUnit.packetInfo);
                        
                        pkt->read(preRead,buffer);
                        // unescape...
                        ADM_unescapeH264(preRead,buffer,header);
                        //
                        getBits bits(preRead,header);
                        
                        thisUnit.imageType=decodePictureTypeH265(startCode,bits);
                        
                        if(!addUnit(data,unitTypePic,thisUnit,startCodeLength+NON_IDR_PRE_READ))
                            keepRunning=false;
                            // reset to default
                        thisUnit.imageStructure=pictureFrame;
                        thisUnit.recoveryCount=0xff;
                        pkt->invalidatePtsDts();
                }
                    break;
                  case NAL_H265_SPS:
                                decodingImage=false;
                                pkt->getInfo(&thisUnit.packetInfo);
                                if(firstSps)
                                {
                                    pkt->setConsumed(startCodeLength); // reset consume counter
                                    firstSps=false;
                                }
                                thisUnit.consumedSoFar=pkt->getConsumed();
                                
                                if(!addUnit(data,unitTypeSps,thisUnit,startCodeLength))
                                    keepRunning=false;
                          break;                
                  default:
                      break;
          }
      } // End while
      result=true;
the_end:
        printf("\n");
        qfprintf(index,"\n[End]\n");
        qfclose(index);
        index=NULL;
        audioTracks=NULL;
        delete pkt;
        pkt=NULL;
        return result;
}


//

//EOF