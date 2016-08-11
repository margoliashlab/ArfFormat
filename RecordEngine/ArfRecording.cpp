/*
 ------------------------------------------------------------------

 Michal Badura, 2016
 based on code by Florian Franzen, 2014

 ------------------------------------------------------------------

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#include "ArfRecording.h"
#include "../../../Processors/GenericProcessor/GenericProcessor.h"

#define MAX_BUFFER_SIZE 40960
#define CHANNEL_TIMESTAMP_PREALLOC_SIZE 128
#define CHANNEL_TIMESTAMP_MIN_WRITE	32

// no parts in this version
#define TIMESTAMP_EACH_NSAMPLES 0

ArfRecording::ArfRecording() : processorIndex(-1), bufferSize(MAX_BUFFER_SIZE), hasAcquired(false)
{
    //timestamp = 0;
    scaledBuffer.malloc(MAX_BUFFER_SIZE);
    intBuffer.malloc(MAX_BUFFER_SIZE);
    savingNum = SAVING_NUM; //declared as a const in ArfRecording.h
    cntPerPart = 1000;
    partNo = 0;
    partCnt = 0;
}

ArfRecording::~ArfRecording()
{	
}

String ArfRecording::getEngineID()
{
    return "Arf";
}


void ArfRecording::registerProcessor(GenericProcessor* proc)
{
    ArfRecordingInfo* info = new ArfRecordingInfo();
    info->sample_rate = proc->getSampleRate();
    info->bit_depth = 16;
    info->multiSample = false;
    infoArray.add(info);
    fileArray.add(new ArfFile());
    bitVoltsArray.add(new Array<float>);
    sampleRatesArray.add(new Array<float>);
	channelsPerProcessor.add(0);
    processorIndex++;
}

void ArfRecording::resetChannels()
{
	scaledBuffer.malloc(MAX_BUFFER_SIZE);
	intBuffer.malloc(MAX_BUFFER_SIZE);
	bufferSize = MAX_BUFFER_SIZE;
    processorIndex = -1;
    fileArray.clear();
	channelsPerProcessor.clear();
    bitVoltsArray.clear();
    sampleRatesArray.clear();
    processorMap.clear();
    infoArray.clear();
	recordedChanToKWDChan.clear();
	channelLeftOverSamples.clear();
	channelTimestampArray.clear();

    for (int i=0; i<partBuffer.size(); i++) 
    {
        partBuffer[i]->clear();
    }
    partNo = 0;
    partCnt = 0;
}

void ArfRecording::addChannel(int index, Channel* chan)
{
    processorMap.add(processorIndex);
}


void ArfRecording::openFiles(File rootFolder, int experimentNumber, int recordingNumber)
{
    this->rootFolder = rootFolder;
    this->experimentNumber = experimentNumber;
    this->recordingNumber = recordingNumber;
    String partName = "";
    if (savingNum != 0) {
        partName = "_prt"+String(partNo);
        std::cout << "Opening part" << partNo << std::endl;
    }
    String basepath = rootFolder.getFullPathName() + rootFolder.separatorString + "experiment" + String(experimentNumber) + partName;

	recordedChanToKWDChan.clear();
	Array<int> processorRecPos;
	processorRecPos.insertMultiple(0, 0, fileArray.size());
	
    mainInfo = new ArfRecordingInfo();
    mainFile = new ArfFile();
    
    
    mainFile->initFile(0, basepath);
    if (hasAcquired)
        mainInfo->start_time = (*timestamps)[getChannel(0)->sourceNodeId]; //(*timestamps).begin()->first;
    else
        mainInfo->start_time = 0;  

    for (int i = 0; i < processorMap.size(); i++)
    {
        int index = processorMap[i];
        if (getChannel(i)->getRecordState())
        {
            recordedChanToKWDChan.add(channelsPerProcessor[index]);
            // processorRecPos.set(i, channelsPerProcessor[index]);
            channelsPerProcessor.set(index, channelsPerProcessor[index] + 1);
            bitVolts.add(getChannel(i)->bitVolts);
            sampleRates.add(getChannel(i)->sampleRate);

            procMap.add(getChannel(i)->nodeId);
            // int procPos = processorRecPos[processorMap[i]];
            
            // std::cout << "i " << i << " nodeid " << getChannel(i)->nodeId << " procPos " << procPos << " processorMap[i]" << processorMap[i] << std::endl;
            
            channelTimestampArray.add(new Array<int64>);
            channelTimestampArray.getLast()->ensureStorageAllocated(CHANNEL_TIMESTAMP_PREALLOC_SIZE);
            channelLeftOverSamples.add(0);
        }
    }
    
    mainFile->open(processorMap.size());
    
    mainInfo->name = String("Open Ephys Recording #") + String(recordingNumber);
    mainInfo->start_sample = 0;
    mainInfo->sample_rate = infoArray[0]->sample_rate;
    mainInfo->bitVolts.clear();
    mainInfo->bitVolts.addArray(bitVolts);
    mainInfo->channelSampleRates.clear();
    mainInfo->channelSampleRates.addArray(sampleRates);
        
    mainFile->addEventType("TTL",ArfFileBase::U8,"event_channels");
    mainFile->addEventType("Messages",ArfFileBase::STR,"Text");
    
    for (int i=0; i<spikeInfoArray.size(); i++)
    {
        mainFile->addChannelGroup(spikeInfoArray[i]);        
    }
    
    mainFile->startNewRecording(recordingNumber,processorMap.size(), mainInfo, recordedChanToKWDChan, procMap);   
    
    for (int i=0; i<processorMap.size(); i++)
    {        
        partBuffer.add(new Array<int16, CriticalSection, 3*SAVING_NUM>);
    }

    hasAcquired = true;
}

void ArfRecording::closeFiles()
{    
    //TODO There are some unsaved samples in partBuf when we stop recording. However, only savingNum of them at most.
    
    mainFile->stopRecording();
    mainFile->close();
    bitVolts.clear();
    sampleRates.clear();

	recordedChanToKWDChan.clear();
	channelTimestampArray.clear();
	channelLeftOverSamples.clear();
	scaledBuffer.malloc(MAX_BUFFER_SIZE);
	intBuffer.malloc(MAX_BUFFER_SIZE);
	bufferSize = MAX_BUFFER_SIZE;
}

void ArfRecording::writeData(AudioSampleBuffer& buffer)
{        	
    for (int i = 0; i < buffer.getNumChannels(); i++)
    {
        if (getChannel(i)->getRecordState())
        {

            int sourceNodeId = getChannel(i)->sourceNodeId;
            int nSamples = (*numSamples)[sourceNodeId];

            double multFactor = 1 / (float(0x7fff) * getChannel(i)->bitVolts);
            int index = processorMap[getChannel(i)->recordIndex];
            FloatVectorOperations::copyWithMultiply(scaledBuffer.getData(), buffer.getReadPointer(i,0), multFactor, nSamples);
            AudioDataConverters::convertFloatToInt16LE(scaledBuffer.getData(), intBuffer.getData(), nSamples);
            mainFile->writeChannel(intBuffer.getData(), nSamples, i);
        }
    }
}

void ArfRecording::endChannelBlock(bool lastBlock)
{

}

void ArfRecording::writeEvent(int eventType, MidiMessage& event, int samplePosition)
{
    
    //Currently timestamp is general, not relative to the current part.
    //Maybe you need to subtract how much samples have passed
    ScopedLock sl(partLock);
    const uint8* dataptr = event.getRawData();
    if (eventType == GenericProcessor::TTL)
    {
        mainFile->writeEvent(0,*(dataptr+2),*(dataptr+1),(void*)(dataptr+3),(*timestamps)[*(dataptr+1)]+samplePosition);
    }
        
    else if (eventType == GenericProcessor::MESSAGE)
    {
        String msg = String((char*)(dataptr+6));
        if (msg.startsWith("ARF"))
        {
            processSpecialEvent(msg);
            //TODO add messages with that override the timestamp
            //but that requires knowing the actual recording start time
        }
        else
        {
            mainFile->writeEvent(1,*(dataptr+2),*(dataptr+1),(void*)(dataptr+6),(*timestamps)[*(dataptr+1)]+samplePosition);
        }
    }
}

void ArfRecording::processSpecialEvent(String msg)
{
    StringArray words = StringArray();
    words.addTokens(msg, " ", "\"");
    std::cout << words[1] << std::endl;
    if (words[1].compare("SetAttr")==0)
    {
        mainFile->setAttributeStr(words[3], "/rec_"+String(recordingNumber), words[2]);
    }
    else if(words[1].compare("TS")==0)
    {
        int64 ts = words[2].getLargeIntValue();
        String msg = words[3];
        std::cout << ts << msg << std::endl;
    }
    
}

void ArfRecording::addSpikeElectrode(int index, SpikeRecordInfo* elec)
{
    spikeInfoArray.add(elec->numChannels);
}
void ArfRecording::writeSpike(const SpikeObject& spike, int electrodeIndex)
{
    int64 timestamp = spike.timestamp;
    float time = (float)timestamp / spike.samplingFrequencyHz;
    // What if multiple parts? Maybe you need to subtract how much samples have passed
    ScopedLock sl(partLock);    
    mainFile->writeSpike(electrodeIndex,spike.nSamples,spike.data,time);
}

void ArfRecording::startAcquisition()
{
    
}

RecordEngineManager* ArfRecording::getEngineManager()
{
    RecordEngineManager* man = new RecordEngineManager("Arf","Arf",&(engineFactory<ArfRecording>));
    return man;
}
