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

#ifndef ARFRECORDING_H_INCLUDED
#define ARFRECORDING_H_INCLUDED

#include <RecordingLib.h>
#include "ArfFileFormat.h"

#define SAVING_NUM 20000

class ArfRecording : public RecordEngine
{
public:
    ArfRecording();
    ~ArfRecording();
    String getEngineID();
    void openFiles(File rootFolder, int experimentNumber, int recordingNumber);
	void closeFiles();
	void writeData(AudioSampleBuffer& buffer);
	void writeEvent(int eventType, MidiMessage& event, int samplePosition);
	void addChannel(int index, Channel* chan);
	void addSpikeElectrode(int index, SpikeRecordInfo* elec);
	void writeSpike(const SpikeObject& spike, int electrodeIndex);
	void registerProcessor(GenericProcessor* processor);
	void resetChannels();
	void startAcquisition();
	void endChannelBlock(bool lastBlock);

    static RecordEngineManager* getEngineManager();
private:

    int processorIndex;
    
    void processSpecialEvent(String msg);

    Array<int> processorMap;
	Array<int> channelsPerProcessor;
	Array<int> recordedChanToKWDChan;
    OwnedArray<Array<float>> bitVoltsArray;
    OwnedArray<Array<float>> sampleRatesArray;
	OwnedArray<Array<int64>> channelTimestampArray;
    
    Array<float> bitVolts;
    Array<float> sampleRates;
    Array<int> procMap;
    
	Array<int> channelLeftOverSamples;
    OwnedArray<ArfFile> fileArray;
    OwnedArray<ArfRecordingInfo> infoArray;
	HeapBlock<float> scaledBuffer;
	HeapBlock<int16> intBuffer;
	int bufferSize;    
    
    Array<int> spikeInfoArray;
    
    ScopedPointer<ArfFile> mainFile;
    ScopedPointer<ArfRecordingInfo> mainInfo;

    int savingNum;
    
    //To ensure always at least 3*SAVING_NUM is allocated in all of the arrays
    OwnedArray<Array<int16, CriticalSection, 3*SAVING_NUM>, CriticalSection> partBuffer;
    int partNo;
    int partCnt;
    int cntPerPart;
    CriticalSection partLock;
    
    File rootFolder;
    int experimentNumber;
    int recordingNumber;

    bool hasAcquired;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArfRecording);
};


#endif  // ARFRECORDING_H_INCLUDED
