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
#define TIMESTAMP_EACH_NSAMPLES 1024

ArfRecording::ArfRecording() : processorIndex(-1), bufferSize(MAX_BUFFER_SIZE), hasAcquired(false)
{
    //timestamp = 0;
    scaledBuffer.malloc(MAX_BUFFER_SIZE);
    intBuffer.malloc(MAX_BUFFER_SIZE);
    savingNum = 10000;
    cntPerPart = 1000;
    partNo = 0;
    partCnt = 0;
}

ArfRecording::~ArfRecording()
{	
}

String ArfRecording::getEngineID() const
{
    return "Arf";
}


void ArfRecording::registerProcessor(const GenericProcessor* proc)
{
    ArfRecordingInfo* info = new ArfRecordingInfo();
	//This is a VERY BAD thig to do. temporary only until we fix const-correctness on GenericEditor methods
	//(which implies modifying all the plugins and processors)
    info->sample_rate = const_cast<GenericProcessor*>(proc)->getSampleRate();
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
    if (spikesFile)
        spikesFile->resetChannels();
    for (int i=0; i<partBuffer.size(); i++) 
    {
        partBuffer[i]->clear();
    }
    partNo = 0;
    partCnt = 0;
}

void ArfRecording::addChannel(int index,const Channel* chan)
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
    eventFile->initFile(basepath);
    eventFile->open();

    spikesFile->initFile(basepath);
    spikesFile->open();
    spikesFile->startNewRecording(recordingNumber);

    //Let's just put the first processor (usually the source node) on the KWIK for now
    infoArray[0]->name = String("Open Ephys Recording #") + String(recordingNumber);

	infoArray[0]->start_time = getTimestamp(0);

    infoArray[0]->start_sample = 0;
    eventFile->startNewRecording(recordingNumber,infoArray[0]);

	recordedChanToKWDChan.clear();
	Array<int> processorRecPos;
	processorRecPos.insertMultiple(0, 0, fileArray.size());
	for (int i = 0; i < getNumRecordedChannels(); i++)
	{
		int index = processorMap[getRealChannel(i)];
		ArfFile* newFile = new ArfFile;
        fileArray.set(index, newFile);
        if (!fileArray[index]->isOpen())
		{
			fileArray[index]->initFile(getChannel(getRealChannel(i))->nodeId, basepath);
			infoArray[index]->start_time = getTimestamp(i);
		}

		channelsPerProcessor.set(index, channelsPerProcessor[index] + 1);
		bitVoltsArray[index]->add(getChannel(getRealChannel(i))->bitVolts);
		sampleRatesArray[index]->add(getChannel(getRealChannel(i))->sampleRate);
		if (getChannel(getRealChannel(i))->sampleRate != infoArray[index]->sample_rate)
		{
			infoArray[index]->multiSample = true;
		}
		int procPos = processorRecPos[index];
		recordedChanToKWDChan.add(procPos);
		processorRecPos.set(index, procPos+1);
		channelTimestampArray.add(new Array<int64>);
		channelTimestampArray.getLast()->ensureStorageAllocated(CHANNEL_TIMESTAMP_PREALLOC_SIZE);
		channelLeftOverSamples.add(0);
	} 

    for (int i = 0; i < fileArray.size(); i++)
    {
		if ((!fileArray[i]->isOpen()) && (fileArray[i]->isReadyToOpen()))
		{
			fileArray[i]->open(channelsPerProcessor[i]);
		}
        if (fileArray[i]->isOpen())
        {

            infoArray[i]->name = String("Open Ephys Recording #") + String(recordingNumber);
            infoArray[i]->start_sample = 0;
            infoArray[i]->bitVolts.clear();
            infoArray[i]->bitVolts.addArray(*bitVoltsArray[i]);
            infoArray[i]->channelSampleRates.clear();
            infoArray[i]->channelSampleRates.addArray(*sampleRatesArray[i]);
            fileArray[i]->startNewRecording(recordingNumber,bitVoltsArray[i]->size(),infoArray[i]);
        }
    }
    
    for (int i=0; i<getNumRecordedChannels(); i++)
    {        
        partBuffer.add(new Array<int16>);
    }

    hasAcquired = true;
}

void ArfRecording::closeFiles()
{    
    eventFile->stopRecording();
    eventFile->close();
    spikesFile->stopRecording();
    spikesFile->close();
    //TODO There are some unsaved samples in partBuf when we stop recording. However, only savingNum of them at most.
    for (int i = 0; i < fileArray.size(); i++)
    {
        if (fileArray[i]->isOpen())
        {
			std::cout << "Closed file " << i << std::endl;
            fileArray[i]->stopRecording();
            fileArray[i]->close();
            bitVoltsArray[i]->clear();
			sampleRatesArray[i]->clear();
        }
		channelsPerProcessor.set(i, 0);
    }
	recordedChanToKWDChan.clear();
	channelTimestampArray.clear();
	channelLeftOverSamples.clear();
	scaledBuffer.malloc(MAX_BUFFER_SIZE);
	intBuffer.malloc(MAX_BUFFER_SIZE);
	bufferSize = MAX_BUFFER_SIZE;
}

void ArfRecording::writeData(int writeChannel, int realChannel, const float* buffer, int size)
{        
    if (size > bufferSize) //Shouldn't happen, and if it happens it'll be slow, but better this than crashing. Will be reset on flie close and reset.
	{
		std::cerr << "Write buffer overrun, resizing to" << size << std::endl;
		bufferSize = size;
		scaledBuffer.malloc(size);
		intBuffer.malloc(size);
	}
	double multFactor = 1 / (float(0x7fff) * getChannel(realChannel)->bitVolts);
	int index = processorMap[getChannel(realChannel)->recordIndex];
	FloatVectorOperations::copyWithMultiply(scaledBuffer.getData(), buffer, multFactor, size);
	AudioDataConverters::convertFloatToInt16LE(scaledBuffer.getData(), intBuffer.getData(), size);
    
    if (savingNum != 0) { //saving in parts; based on intermediate buffer
        int16* buf = intBuffer.getData();
        //simply appending to Array - best option?
        for (int i=0; i<size; i++) {
            partBuffer[writeChannel]->add(*(buf+i));
        }

        const ScopedLock al(partBuffer.getLock());
        bool ifSave = true;
        for (int i=0; i<getNumRecordedChannels();i++)
        {
            if (partBuffer[i]->size() < savingNum)
                ifSave = false;
        }
        if (ifSave) 
        {
            if (partCnt >= cntPerPart) {
                //This lock is also in writeEvent, writeSpike.
                //Should prevent from trying to write one of those when we are opening the next part.
                ScopedLock sl(partLock);
                partNo++;
                this->closeFiles();
                this->openFiles(rootFolder, experimentNumber, recordingNumber);
                partCnt = 0;
            }
            partCnt++;
        
            for (int i=0; i<getNumRecordedChannels();i++)
            {
                int idx = processorMap[getChannel(i)->recordIndex];
                int write_idx = recordedChanToKWDChan[i];
                fileArray[index]->writeChannel(partBuffer[i]->getRawDataPointer(), savingNum, write_idx);
                partBuffer[i]->removeRange(0, savingNum);            
            }                    
        }
    }
    else { //saving to one file
        fileArray[index]->writeChannel(intBuffer.getData(), size, recordedChanToKWDChan[writeChannel]);
    }

}

void ArfRecording::endChannelBlock(bool lastBlock)
{

}

void ArfRecording::writeEvent(int eventType, const MidiMessage& event, int64 timestamp)
{
    //TODO makes no sense to lock before processing special message
    ScopedLock sl(partLock);
    const uint8* dataptr = event.getRawData();
    if (eventType == GenericProcessor::TTL)
    {
        eventFile->writeEvent(0,*(dataptr+2),*(dataptr+1),(void*)(dataptr+3),timestamp);
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
            eventFile->writeEvent(1,*(dataptr+2),*(dataptr+1),(void*)(dataptr+6),timestamp);
        
        }
    }
}

void ArfRecording::processSpecialEvent(String msg)
{
    StringArray words = StringArray();
    words.addTokens(msg, " ", "\"");
    //TODO probably better to set attributes of files in fileArray?
    std::cout << words[1] << std::endl;
    if (words[1].compare("SetAttr"))
    {
        eventFile->setAttributeStr(words[3], "/event_types", words[2]);        
    }
    else if(words[1].compare("TS"))
    {
        int64 ts = words[2].getLargeIntValue();
        String msg = words[3];
        std::cout << ts << msg << std::endl;
    }
    
}

void ArfRecording::addSpikeElectrode(int index, const SpikeRecordInfo* elec)
{
    spikesFile->addChannelGroup(elec->numChannels);
}
void ArfRecording::writeSpike(int electrodeIndex, const SpikeObject& spike, int64 /*timestamp*/)
{
    ScopedLock sl(partLock);
    float time = (float)spike.timestamp / spike.samplingFrequencyHz;
    spikesFile->writeSpike(electrodeIndex,spike.nSamples,spike.data,time);
}

void ArfRecording::startAcquisition()
{
    eventFile = new AEFile();
    eventFile->addEventType("TTL",ArfFileBase::U8,"event_channels");
    eventFile->addEventType("Messages",ArfFileBase::STR,"Text");
    spikesFile = new AXFile();
}

RecordEngineManager* ArfRecording::getEngineManager()
{
    RecordEngineManager* man = new RecordEngineManager("Arf","Arf",&(engineFactory<ArfRecording>));
    return man;
}