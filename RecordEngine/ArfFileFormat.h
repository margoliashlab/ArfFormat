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
#define MAX_STR_SIZE 256
#define MAX_TRANSFORM_SIZE 512
#ifndef ARFFILEFORMAT_H_INCLUDED
#define ARFFILEFORMAT_H_INCLUDED

#include "../../../../JuceLibraryCode/JuceHeader.h"

class ArfRecordingData;
namespace H5
{
class DataSet;
class H5File;
class DataType;
class CompType;
class ArrayType;

}

struct ArfRecordingInfo
{
    String name;
    int64 start_time;
    uint32 start_sample;
    float sample_rate;
    uint32 bit_depth;
    Array<float> bitVolts;
    Array<float> channelSampleRates;
    bool multiSample;
};

class ArfFileBase
{
public:
    ArfFileBase();
    virtual ~ArfFileBase();

    int open();
	int open(int nChans);
    void close();
    virtual String getFileName() = 0;
    bool isOpen() const;
	bool isReadyToOpen() const;
    typedef enum DataTypes { U8, U16, U32, U64, I8, I16, I32, I64, F32, STR} DataTypes;

    static H5::DataType getNativeType(DataTypes type);
    static H5::DataType getH5Type(DataTypes type);
    
    
protected:

    virtual int createFileStructure() = 0;

    int setAttribute(DataTypes type, void* data, String path, String name);
    int setAttributeStr(String value, String path, String name);
    int setAttributeAsArray(DataTypes type, void* data, int size, String path, String name);
    int setAttributeArray(DataTypes type, void* data, int size, String path, String name);
    int createGroup(String path);

    ArfRecordingData* getDataSet(String path);

    //aliases for createDataSet
    ArfRecordingData* createDataSet(DataTypes type, int sizeX, int chunkX, String path);
    ArfRecordingData* createDataSet(DataTypes type, int sizeX, int sizeY, int chunkX, String path);
    ArfRecordingData* createDataSet(DataTypes type, int sizeX, int sizeY, int sizeZ, int chunkX, String path);
    ArfRecordingData* createDataSet(DataTypes type, int sizeX, int sizeY, int sizeZ, int chunkX, int chunkY, String path);
    ArfRecordingData* createCompoundDataSet(H5::CompType type, String path, int dimension, int* max_dims, int* chunk_dims);

    bool readyToOpen;

private:
    //create an extendable dataset
    ArfRecordingData* createDataSet(DataTypes type, int dimension, int* size, int* chunking, String path);
    int open(bool newfile, int nChans);
    ScopedPointer<H5::H5File> file;
    bool opened;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArfFileBase);
};

class ArfRecordingData
{
public:
    ArfRecordingData(H5::DataSet* data);
    ~ArfRecordingData();

    int writeDataBlock(int xDataSize, ArfFileBase::DataTypes type, void* data);
    int writeDataBlock(int xDataSize, int yDataSize, ArfFileBase::DataTypes type, void* data);

    int writeDataRow(int yPos, int xDataSize, ArfFileBase::DataTypes type, void* data);
    
    int writeDataChannel(int dataSize, ArfFileBase::DataTypes type, void* data);
    
    void writeCompoundData(int xDataSize, int yDataSize, H5::DataType type, void* data);

    void getRowXPositions(Array<uint32>& rows);

private:
    int xPos;
    int xChunkSize;
    int size[3];
    int dimension;
    Array<uint32> rowXPos;
    ScopedPointer<H5::DataSet> dSet;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArfRecordingData);
};

class ArfFile : public ArfFileBase
{
public:
    ArfFile(int processorNumber, String basename);
    ArfFile();
    virtual ~ArfFile();
    void initFile(int processorNumber, String basename);
    void startNewRecording(int recordingNumber, int nChannels, ArfRecordingInfo* info);
    void stopRecording();
    void writeBlockData(int16* data, int nSamples);
    void writeRowData(int16* data, int nSamples);
	void writeRowData(int16* data, int nSamples, int channel);
    void writeChannel(int16* data, int nSamples, int noChannel);
	void writeTimestamps(int64* ts, int nTs, int channel);
    String getFileName();

protected:
    int createFileStructure();

private:
    int recordingNumber;
    int nChannels;
    int curChan;
    String filename;
    bool multiSample;
    ScopedPointer<ArfRecordingData> recdata;
	ScopedPointer<ArfRecordingData> tsData;
    
    OwnedArray<ArfRecordingData> recarr;
    
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArfFile);
};

class AEFile : public ArfFileBase
{
public:
    AEFile(String basename);
    AEFile();
    virtual ~AEFile();
    void initFile(String basename);
    void startNewRecording(int recordingNumber, ArfRecordingInfo* info);
    void stopRecording();
    void writeEvent(int type, uint8 id, uint8 processor, void* data, uint64 timestamp);
    void addEventType(String name, DataTypes type, String dataName);
    String getFileName();


protected:
    int createFileStructure();

private:
    typedef struct MessageEvent {
        float time;
        int32 recording;
        uint8 eventID;
        uint8 nodeID;
        char text[MAX_STR_SIZE];        
    } MessageEvent;
    typedef struct TTLEvent {
        float time;
        int32 recording;
        uint8 eventID;
        uint8 nodeID;
        uint8 event_channel;        
    } TTLEvent;
   
    int recordingNumber;
    float sample_rate;
    String filename;
    
    OwnedArray<ArfRecordingData> eventFullData;
    Array<String> eventNames;
    Array<DataTypes> eventTypes;
    Array<String> eventDataNames;
    
    Array<int> eventSizes;
    Array<H5::CompType> eventCompTypes;
    
    int kwdIndex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AEFile);
};

class AXFile : public ArfFileBase
{
public:
    AXFile(String basename);
    AXFile();
    virtual ~AXFile();
    void initFile(String basename);
    void startNewRecording(int recordingNumber);
    void stopRecording();
    void addChannelGroup(int nChannels);
    void resetChannels();
    void writeSpike(int groupIndex, int nSamples, const uint16* data, float time);
    String getFileName();

protected:
    int createFileStructure();

private:
    int createChannelGroup(int index);
    int recordingNumber;
    String filename;
    OwnedArray<ArfRecordingData> spikeArray;
    OwnedArray<ArfRecordingData> recordingArray;
    OwnedArray<ArfRecordingData> timeStamps;
    
    OwnedArray<ArfRecordingData> spikeFullDataArray;
    
    Array<int> channelArray;
    int numElectrodes;
	HeapBlock<int16> transformVector;
    
    typedef struct SpikeInfo {
        float time;
        int recording;
        int16 waveform[MAX_TRANSFORM_SIZE];
        int samples;
    } SpikeInfo;
    Array<H5::CompType> spikeCompTypes;
    SpikeInfo spikeinfo;
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AXFile);
};

#endif  // ARFFILEFORMAT_H_INCLUDED

