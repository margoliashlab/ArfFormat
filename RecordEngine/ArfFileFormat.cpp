/*
 ------------------------------------------------------------------

 This file is part of the Open Ephys GUI
 Copyright (C) 2014 Florian Franzen

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

#include <H5Cpp.h>
#include "ArfFileFormat.h"

#ifndef CHUNK_XSIZE
#define CHUNK_XSIZE 2048
#endif

#ifndef EVENT_CHUNK_SIZE
#define EVENT_CHUNK_SIZE 8
#endif

#ifndef SPIKE_CHUNK_XSIZE
#define SPIKE_CHUNK_XSIZE 8
#endif

#ifndef SPIKE_CHUNK_YSIZE
#define SPIKE_CHUNK_YSIZE 40
#endif

#ifndef TIMESTAMP_CHUNK_SIZE
#define TIMESTAMP_CHUNK_SIZE 16
#endif

//#define MAX_TRANSFORM_SIZE 512 //defined in .h file

//#define MAX_STR_SIZE 256 //defined in .h file

#define ARF_VERSION "2.1"

#define PROCESS_ERROR std::cerr << error.getCDetailMsg() << std::endl; return -1
#define CHECK_ERROR(x) if (x) std::cerr << "Error at HDFRecording " << __LINE__ << std::endl;

using namespace H5;

//HDF5FileBase

ArfFileBase::ArfFileBase() : readyToOpen(false), opened(false)
{
    Exception::dontPrint();
};

ArfFileBase::~ArfFileBase()
{
    close();
}

bool ArfFileBase::isOpen() const
{
    return opened;
}

bool ArfFileBase::isReadyToOpen() const
{
	return readyToOpen;
}

int ArfFileBase::open()
{
	return open(-1);
}

int ArfFileBase::open(int nChans)
{
    if (!readyToOpen) return -1;
    if (File(getFileName()).existsAsFile())
        return open(false, nChans);
    else
        return open(true, nChans);

}

int ArfFileBase::open(bool newfile, int nChans)
{
    int accFlags,ret=0;

    if (opened) return -1;

    try
    {
		FileAccPropList props = FileAccPropList::DEFAULT;
		if (nChans > 0)
		{
			props.setCache(0, 1667, 2 * 8 * 2 * CHUNK_XSIZE * nChans, 1);
			//std::cout << "opening HDF5 " << getFileName() << " with nchans: " << nChans << std::endl;
		}

        if (newfile) accFlags = H5F_ACC_TRUNC;
        else accFlags = H5F_ACC_RDWR;
        file = new H5File(getFileName().toUTF8(),accFlags,FileCreatPropList::DEFAULT,props);
        opened = true;
        if (newfile)
        {
            ret = createFileStructure();
        }

        if (ret)
        {
            file = nullptr;
            opened = false;
            std::cerr << "Error creating file structure" << std::endl;
        }


        return ret;
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }
}

void ArfFileBase::close()
{
    file = nullptr;
    opened = false;
}

int ArfFileBase::setAttribute(DataTypes type, void* data, String path, String name)
{
    return setAttributeArray(type, data, 1, path, name);
}


//Create array of type TYPE, versus setAttributeArray that creates a scalar of type array
int ArfFileBase::setAttributeAsArray(DataTypes type, void* data, int size, String path, String name)
{
    H5Location* loc;
    Group gloc;
    DataSet dloc;
    Attribute attr;
    DataType H5type;
    DataType origType;

    if (!opened) return -1;
    try
    {
        try
        {
            gloc = file->openGroup(path.toUTF8());
            loc = &gloc;
        }
        catch (FileIException error) //If there is no group with that path, try a dataset
        {
            dloc = file->openDataSet(path.toUTF8());
            loc = &dloc;
        }

        H5type = getH5Type(type);
        origType = getNativeType(type);

        hsize_t dims = size;

        if (loc->attrExists(name.toUTF8()))
        {
            attr = loc->openAttribute(name.toUTF8());
        }
        else
        {
            DataSpace attr_dataspace(1, &dims); //create a 1d simple dataspace of len SIZE
            attr = loc->createAttribute(name.toUTF8(),H5type,attr_dataspace);
        }

        attr.write(origType,data);

    }
    catch (GroupIException error)
    {
        PROCESS_ERROR;
    }
    catch (AttributeIException error)
    {
        PROCESS_ERROR;
    }
    catch (DataSetIException error)
    {
        PROCESS_ERROR;
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }

    return 0;
}

int ArfFileBase::setAttributeArray(DataTypes type, void* data, int size, String path, String name)
{
    H5Location* loc;
    Group gloc;
    DataSet dloc;
    Attribute attr;
    DataType H5type;
    DataType origType;

    if (!opened) return -1;
    try
    {
        try
        {
            gloc = file->openGroup(path.toUTF8());
            loc = &gloc;
        }
        catch (FileIException error) //If there is no group with that path, try a dataset
        {
            dloc = file->openDataSet(path.toUTF8());
            loc = &dloc;
        }

        H5type = getH5Type(type);
        origType = getNativeType(type);

        if (size > 1)
        {
            hsize_t dims = size;
            H5type = ArrayType(H5type,1,&dims);
            origType = ArrayType(origType,1,&dims);
        }

        if (loc->attrExists(name.toUTF8()))
        {
            attr = loc->openAttribute(name.toUTF8());
        }
        else
        {
            DataSpace attr_dataspace(H5S_SCALAR);
            attr = loc->createAttribute(name.toUTF8(),H5type,attr_dataspace);
        }

        attr.write(origType,data);

    }
    catch (GroupIException error)
    {
        PROCESS_ERROR;
    }
    catch (AttributeIException error)
    {
        PROCESS_ERROR;
    }
    catch (DataSetIException error)
    {
        PROCESS_ERROR;
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }

    return 0;
}

int ArfFileBase::setAttributeStr(String value, String path, String name)
{
    H5Location* loc;
    Group gloc;
    DataSet dloc;
    Attribute attr;

    if (!opened) return -1;

    StrType type(PredType::C_S1, value.length());
    try
    {
        try
        {
            gloc = file->openGroup(path.toUTF8());
            loc = &gloc;
        }
        catch (FileIException error) //If there is no group with that path, try a dataset
        {
            dloc = file->openDataSet(path.toUTF8());
            loc = &dloc;
        }

        if (loc->attrExists(name.toUTF8()))
        {
            //attr = loc.openAttribute(name.toUTF8());
            return -1; //string attributes cannot change size easily, better not allow overwritting.
        }
        else
        {
            DataSpace attr_dataspace(H5S_SCALAR);
            attr = loc->createAttribute(name.toUTF8(), type, attr_dataspace);
        }
        attr.write(type,value.toUTF8());

    }
    catch (GroupIException error)
    {
        PROCESS_ERROR;
    }
    catch (AttributeIException error)
    {
        PROCESS_ERROR;
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }
    catch (DataSetIException error)
    {
        PROCESS_ERROR;
    }


    return 0;
}

int ArfFileBase::createGroup(String path)
{
    if (!opened) return -1;
    try
    {
        file->createGroup(path.toUTF8());
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }
    catch (GroupIException error)
    {
        PROCESS_ERROR;
    }
    return 0;
}

ArfRecordingData* ArfFileBase::getDataSet(String path)
{
    ScopedPointer<DataSet> data;

    if (!opened) return nullptr;

    try
    {
        data = new DataSet(file->openDataSet(path.toUTF8()));
        return new ArfRecordingData(data.release());
    }
    catch (DataSetIException error)
    {
        error.printError();
        return nullptr;
    }
    catch (FileIException error)
    {
        error.printError();
        return nullptr;
    }
    catch (DataSpaceIException error)
    {
        error.printError();
        return nullptr;
    }
}

ArfRecordingData* ArfFileBase::createDataSet(DataTypes type, int sizeX, int chunkX, String path)
{
    int chunks[3] = {chunkX, 0, 0};
    return createDataSet(type,1,&sizeX,chunks,path);
}

ArfRecordingData* ArfFileBase::createDataSet(DataTypes type, int sizeX, int sizeY, int chunkX, String path)
{
    int size[2];
    int chunks[3] = {chunkX, 0, 0};
    size[0] = sizeX;
    size[1] = sizeY;
    return createDataSet(type,2,size,chunks,path);
}

ArfRecordingData* ArfFileBase::createDataSet(DataTypes type, int sizeX, int sizeY, int sizeZ, int chunkX, String path)
{
    int size[3];
    int chunks[3] = {chunkX, 0, 0};
    size[0] = sizeX;
    size[1] = sizeY;
    size[2] = sizeZ;
    return createDataSet(type,2,size,chunks,path);
}

ArfRecordingData* ArfFileBase::createDataSet(DataTypes type, int sizeX, int sizeY, int sizeZ, int chunkX, int chunkY, String path)
{
    int size[3];
    int chunks[3] = {chunkX, chunkY, 0};
    size[0] = sizeX;
    size[1] = sizeY;
    size[2] = sizeZ;
    return createDataSet(type,3,size,chunks,path);
}

ArfRecordingData* ArfFileBase::createDataSet(DataTypes type, int dimension, int* size, int* chunking, String path)
{
    ScopedPointer<DataSet> data;
    DSetCreatPropList prop;
    if (!opened) return nullptr;

    //Right now this classes don't support datasets with rank > 3.
    //If it's needed in the future we can extend them to be of generic rank
    if ((dimension > 3) || (dimension < 1)) return nullptr;

    DataType H5type = getH5Type(type);

    hsize_t dims[3], chunk_dims[3], max_dims[3];

    for (int i=0; i < dimension; i++)
    {
        dims[i] = size[i];
        if (chunking[i] > 0)
        {
            chunk_dims[i] = chunking[i];
            max_dims[i] = H5S_UNLIMITED;
        }
        else
        {
            chunk_dims[i] = size[i];
            max_dims[i] = size[i];
        }
    }

    try
    {
        DataSpace dSpace(dimension,dims,max_dims);
        prop.setChunk(dimension,chunk_dims);

        data = new DataSet(file->createDataSet(path.toUTF8(),H5type,dSpace,prop));
        return new ArfRecordingData(data.release());
    }
    catch (DataSetIException error)
    {
        error.printError();
        return nullptr;
    }
    catch (FileIException error)
    {
        error.printError();
        return nullptr;
    }
    catch (DataSpaceIException error)
    {
        error.printError();
        return nullptr;
    }


}

//Creates a dataset: an array of HDF5 CompoundType 
//If you want a dimension i to be unlimited, pass chunk_dims[i]=NCHUNK and max_dims[i]=0. If limited, pass max_dims[i]=N and chunk_dims[i]=N.
ArfRecordingData* ArfFileBase::createCompoundDataSet(CompType type, String path, int dimension, int* max_dims, int* chunk_dims)
{
    ScopedPointer<DataSet> data;
    DSetCreatPropList prop;
    
    hsize_t Hdims[3];
    hsize_t Hmax_dims [3];
    hsize_t Hchunk_dims[3];

    for (int i=0; i < dimension; i++)
    {
        Hchunk_dims[i] = chunk_dims[i];
        if (chunk_dims[i] > 0 && chunk_dims[i] != max_dims[i])
        {
            Hmax_dims[i] = H5S_UNLIMITED;
            Hdims[i] = 0;
        }
        else
        {
            Hmax_dims[i] = max_dims[i];
            Hdims[i] = max_dims[i];
        }
    }   
    
    DataSpace dSpace(dimension, Hdims, Hmax_dims);
    prop.setChunk(dimension, Hchunk_dims);
    data = new DataSet(file->createDataSet(path.toUTF8(),type,dSpace,prop));
    return new ArfRecordingData(data.release());  
}

H5::DataType ArfFileBase::getNativeType(DataTypes type)
{
    switch (type)
    {
        case I8:
            return PredType::NATIVE_INT8;
            break;
        case I16:
            return PredType::NATIVE_INT16;
            break;
        case I32:
            return PredType::NATIVE_INT32;
            break;
        case I64:
            return PredType::NATIVE_INT64;
            break;
        case U8:
            return PredType::NATIVE_UINT8;
            break;
        case U16:
            return PredType::NATIVE_UINT16;
            break;
        case U32:
            return PredType::NATIVE_UINT32;
            break;
        case U64:
            return PredType::NATIVE_UINT64;
            break;
        case F32:
            return PredType::NATIVE_FLOAT;
            break;
        case STR:
            return StrType(PredType::C_S1,MAX_STR_SIZE);
            break;
    }
    return PredType::NATIVE_INT32;
}

H5::DataType ArfFileBase::getH5Type(DataTypes type)
{
    switch (type)
    {
        case I8:
            return PredType::STD_I8LE;
            break;
        case I16:
            return PredType::STD_I16LE;
            break;
        case I32:
            return PredType::STD_I32LE;
            break;
        case I64:
            return PredType::STD_I64LE;
            break;
        case U8:
            return PredType::STD_U8LE;
            break;
        case U16:
            return PredType::STD_U16LE;
            break;
        case U32:
            return PredType::STD_U32LE;
            break;
        case U64:
            return PredType::STD_U64LE;
            break;
        case F32:
            return PredType::IEEE_F32LE;
            break;
        case STR:
            return StrType(PredType::C_S1,MAX_STR_SIZE);
            break;
    }
    return PredType::STD_I32LE;
}

ArfRecordingData::ArfRecordingData(DataSet* data)
{
    DataSpace dSpace;
    DSetCreatPropList prop;
    ScopedPointer<DataSet> dataSet = data;
    hsize_t dims[3], chunk[3];

    dSpace = dataSet->getSpace();
    prop = dataSet->getCreatePlist();

    dimension = dSpace.getSimpleExtentDims(dims);
    prop.getChunk(dimension,chunk);

    this->size[0] = dims[0];
    if (dimension > 1)
        this->size[1] = dims[1];
    else
        this->size[1] = 1;
    if (dimension > 1)
        this->size[2] = dims[2];
    else
        this->size[2] = 1;

    this->xChunkSize = chunk[0];
    this->xPos = 0;
    this->dSet = dataSet;
    this->rowXPos.clear();
    this->rowXPos.insertMultiple(0,0,this->size[1]);
}

ArfRecordingData::~ArfRecordingData()
{
	
	dSet->flush(H5F_SCOPE_GLOBAL);
}
int ArfRecordingData::writeDataBlock(int xDataSize, ArfFileBase::DataTypes type, void* data)
{
    return writeDataBlock(xDataSize,size[1],type,data);
}

int ArfRecordingData::writeDataBlock(int xDataSize, int yDataSize, ArfFileBase::DataTypes type, void* data)
{
    hsize_t dim[3],offset[3];
    DataSpace fSpace;
    DataType nativeType;

    dim[2] = size[2];
    //only modify y size if new required size is larger than what we had.
    if (yDataSize > size[1])
        dim[1] = yDataSize;
    else
        dim[1] = size[1];
    dim[0] = xPos + xDataSize;
    try
    {
        //First be sure that we have enough space
        dSet->extend(dim);

        fSpace = dSet->getSpace();
        fSpace.getSimpleExtentDims(dim);
        size[0]=dim[0];
        if (dimension > 1)
            size[1]=dim[1];

        //Create memory space
        dim[0]=xDataSize;
        dim[1]=yDataSize;
        dim[2] = size[2];

        DataSpace mSpace(dimension,dim);
        //select where to write
        offset[0]=xPos;
        offset[1]=0;
        offset[2]=0;

        fSpace.selectHyperslab(H5S_SELECT_SET, dim, offset);

        nativeType = ArfFileBase::getNativeType(type);

        dSet->write(data,nativeType,mSpace,fSpace);
        xPos += xDataSize;
    }
    catch (DataSetIException error)
    {
        PROCESS_ERROR;
    }
    catch (DataSpaceIException error)
    {
        PROCESS_ERROR;
    }
    return 0;
}

//write data into a 1-d array, with type wrapped by ArfFileBase::DataTypes, instead of raw HDF5 type
int ArfRecordingData::writeDataChannel(int dataSize, ArfFileBase::DataTypes type, void* data)
{
    //Data is 1-dimensional
    hsize_t dim[3],offset[3];
    DataSpace fSpace;
    
    DataType nativeType = ArfFileBase::getNativeType(type);
    if (xPos+dataSize > size[0])
    {
        dim[0] = xPos + dataSize;
        dim[1] = 0;
        dim[2] = 0;
        dSet->extend(dim);
    }
    
    fSpace = dSet->getSpace();
    fSpace.getSimpleExtentDims(dim);
    size[0]=dim[0];
    if (dimension > 1)
        size[1]=dim[1];

    //Create memory space
    dim[0]= dataSize;
    dim[1]= 0;
    dim[2]= 0;

    DataSpace mSpace(dimension,dim);
    //select where to write
    offset[0]=xPos;
    offset[1]=0;
    offset[2]=0;

    fSpace.selectHyperslab(H5S_SELECT_SET, dim, offset);
    
    
    dSet->write(data,nativeType,mSpace,fSpace);
    xPos = xPos + dataSize;
}

//Writes data into an array of HDF5 datatype TYPE
void ArfRecordingData::writeCompoundData(int xDataSize, int yDataSize, DataType type, void* data)
{
    hsize_t dim[3],offset[3];
    DataSpace fSpace;
    DataType nativeType;

    dim[2] = size[2];
    //only modify y size if new required size is larger than what we had.
    if (yDataSize > size[1])
        dim[1] = yDataSize;
    else
        dim[1] = size[1];
    dim[0] = xPos + xDataSize;

        //First be sure that we have enough space
        dSet->extend(dim);

        fSpace = dSet->getSpace();
        fSpace.getSimpleExtentDims(dim);
        size[0]=dim[0];
        if (dimension > 1)
            size[1]=dim[1];

        //Create memory space
        dim[0]=xDataSize;
        dim[1]=yDataSize;
        dim[2] = size[2];

        DataSpace mSpace(dimension,dim);
        //select where to write
        offset[0]=xPos;
        offset[1]=0;
        offset[2]=0;

        fSpace.selectHyperslab(H5S_SELECT_SET, dim, offset);

        dSet->write(data,type,mSpace,fSpace);
        xPos += xDataSize;
}


int ArfRecordingData::writeDataRow(int yPos, int xDataSize, ArfFileBase::DataTypes type, void* data)
{
    hsize_t dim[2],offset[2];
    DataSpace fSpace;
    DataType nativeType;
    if (dimension > 2) return -4; //We're not going to write rows in datasets bigger than 2d.
    //    if (xDataSize != rowDataSize) return -2;
    if ((yPos < 0) || (yPos >= size[1])) return -2;

    try
    {
        if (rowXPos[yPos]+xDataSize > size[0])
        {
            dim[1] = size[1];
            dim[0] = rowXPos[yPos] + xDataSize;
            dSet->extend(dim);

            fSpace = dSet->getSpace();
            fSpace.getSimpleExtentDims(dim);
            size[0]=dim[0];
        }
        if (rowXPos[yPos]+xDataSize > xPos)
        {
            xPos = rowXPos[yPos]+xDataSize;
        }

        dim[0] = xDataSize;
        dim[1] = 1;
        DataSpace mSpace(dimension,dim);

        fSpace = dSet->getSpace();
        offset[0] = rowXPos[yPos];
        offset[1] = yPos;
        fSpace.selectHyperslab(H5S_SELECT_SET, dim, offset);

        nativeType = ArfFileBase::getNativeType(type);


        dSet->write(data,nativeType,mSpace,fSpace);

        rowXPos.set(yPos,rowXPos[yPos] + xDataSize);
    }
    catch (DataSetIException error)
    {
        PROCESS_ERROR;
    }
    catch (DataSpaceIException error)
    {
        PROCESS_ERROR;
    }
    catch (FileIException error)
    {
        PROCESS_ERROR;
    }
    return 0;
}

void ArfRecordingData::getRowXPositions(Array<uint32>& rows)
{
    rows.clear();
    rows.addArray(rowXPos);
}

//KWD File

ArfFile::ArfFile(int processorNumber, String basename) : ArfFileBase()
{
    initFile(processorNumber, basename);
}

ArfFile::ArfFile() : ArfFileBase()
{
}

ArfFile::~ArfFile() {}

String ArfFile::getFileName()
{
    return filename;
}

void ArfFile::initFile(int processorNumber, String basename)
{
    if (isOpen()) return;
    filename = basename + "_" + String(processorNumber) + ".arf";
    readyToOpen=true;
}

void ArfFile::startNewRecording(int recordingNumber, int nChannels, ArfRecordingInfo* info)
{
    this->recordingNumber = recordingNumber;
    this->nChannels = nChannels;
    this->multiSample = info->multiSample;
    uint8 mSample = info->multiSample ? 1 : 0;

	ScopedPointer<ArfRecordingData> bitVoltsSet;
	ScopedPointer<ArfRecordingData> sampleRateSet;

    String recordPath = String("/rec_")+String(recordingNumber);
    CHECK_ERROR(createGroup(recordPath));
    CHECK_ERROR(setAttributeStr(info->name,recordPath,String("name")));
    CHECK_ERROR(setAttribute(U32,&(info->bit_depth),recordPath,String("bit_depth")));

    CHECK_ERROR(setAttribute(U8,&mSample,recordPath,String("is_multiSampleRate_data")));

    int64 timeMilli = Time::currentTimeMillis();
    int64 times[2] = {timeMilli/1000, (timeMilli%1000)*1000};
    setAttributeAsArray(I64, times, 2, recordPath, "timestamp");
    
    String uuid = Uuid().toDashedString();
    CHECK_ERROR(setAttributeStr(uuid, recordPath, String("uuid")));
        
    for (int i = 0; i<nChannels; i++) {        
        //separate Dataset for each channel
        String channelPath = recordPath+"/channel"+String(i);
        
        recarr.add(createDataSet(I16, 0, CHUNK_XSIZE, channelPath));
        CHECK_ERROR(setAttribute(F32, info->channelSampleRates.getRawDataPointer()+i, channelPath, String("sampling_rate")));
        CHECK_ERROR(setAttribute(F32, info->bitVolts.getRawDataPointer()+i, channelPath, String("bit_volts")));
        CHECK_ERROR(setAttributeStr(String("V"), channelPath, String("units")));
        int64 datatype = 0;
        CHECK_ERROR(setAttribute(I64,&datatype,channelPath, String("datatype")));
    }
//TODO get rid of timestamps
//	tsData = createDataSet(I64, 0, nChannels, TIMESTAMP_CHUNK_SIZE, recordPath + "/timestamps");
//	if (!tsData.get())
//		std::cerr << "Error creating timestamps data set" << std::endl;

    curChan = nChannels;
}

void ArfFile::stopRecording()
{
    Array<uint32> samples;
//    String path = String("/recordings/")+String(recordingNumber)+String("/data");
//    recdata->getRowXPositions(samples);

//    CHECK_ERROR(setAttributeArray(U32,samples.getRawDataPointer(),samples.size(),path,"valid_samples"));
    //ScopedPointer does the deletion and destructors the closings
    recdata = nullptr;
    recarr.clear();
	tsData = nullptr;
}

int ArfFile::createFileStructure()
{
    if (setAttributeStr(ARF_VERSION, "/", "arf_version")) return -1;
    return 0;
}

void ArfFile::writeBlockData(int16* data, int nSamples)
{
    CHECK_ERROR(recdata->writeDataBlock(nSamples,I16,data));
}

void ArfFile::writeChannel(int16* data, int nSamples, int noChannel)
{
    CHECK_ERROR(recarr[noChannel]->writeDataChannel(nSamples,I16,data));
}

void ArfFile::writeRowData(int16* data, int nSamples)
{
    if (curChan >= nChannels)
    {
        curChan=0;
    }
    CHECK_ERROR(recdata->writeDataRow(curChan,nSamples,I16,data));
    curChan++;
}

void ArfFile::writeRowData(int16* data, int nSamples, int channel)
{
	if (channel >= 0 && channel < nChannels)
	{
		CHECK_ERROR(recdata->writeDataRow(channel, nSamples, I16, data));
		curChan = channel;
	}
}

void ArfFile::writeTimestamps(int64* ts, int nTs, int channel)
{
	if (channel >= 0 && channel < nChannels)
	{
		CHECK_ERROR(tsData->writeDataRow(channel, nTs, I64, ts));
	}
}

//Events file

AEFile::AEFile(String basename) : ArfFileBase()
{
    initFile(basename);
}

AEFile::AEFile() : ArfFileBase()
{

}

AEFile::~AEFile() {}

String AEFile::getFileName()
{
    return filename;
}

void AEFile::initFile(String basename)
{
    if (isOpen()) return;
    filename = basename + "_events.arf";
    readyToOpen=true;
    
}

int AEFile::createFileStructure()
{
    if (setAttributeStr(ARF_VERSION, "/", "arf_version")) return -1;
    if (createGroup("/event_types")) return -1;
    String uuid = Uuid().toDashedString();
    CHECK_ERROR(setAttributeStr(uuid, String("event_types"), String("uuid")));
    for (int i=0; i < eventNames.size(); i++)
    {
        ScopedPointer<ArfRecordingData> dSet;
        String path = "/event_types/";

        int max_dims[3] = {0, 0, 0};
        int chunk_dims[3] = {EVENT_CHUNK_SIZE, 0, 0};
        dSet = createCompoundDataSet(eventCompTypes[i],path + eventNames[i], 1, max_dims, chunk_dims);
        CHECK_ERROR(setAttributeStr(String("samples"), path + eventNames[i], String("units")));
        
    }
    return 0;
}

void AEFile::startNewRecording(int recordingNumber, ArfRecordingInfo* info)
{
    this->recordingNumber = recordingNumber;
    this->sample_rate = info->sample_rate;
    kwdIndex=0;
//    String recordPath = String("/recordings/")+String(recordingNumber);
    for (int i = 0; i < eventNames.size(); i++)
    {
        ArfRecordingData* dSet;
        
        String path = "/event_types/" + eventNames[i];
        
        dSet = getDataSet(path);
        eventFullData.add(dSet);

    }
}

void AEFile::stopRecording()
{
    eventFullData.clear();
}

void AEFile::writeEvent(int type, uint8 id, uint8 processor, void* data, uint64 timestamp)
{
    if (type > eventNames.size() || type < 0)
    {
        std::cerr << "writeEvent Invalid event type " << type << std::endl;
        return;
    }
    
    //Declare here, so that they are still in scope when we call write.
    //This is unfortunately silly. If you add event types, you need to add pointers here.
    MessageEvent evm;
    TTLEvent evt;
    void* evptr;

    float time = timestamp / sample_rate;

    if (eventNames[type] == "Messages")
    {
        evm.time = time;
        evm.recording = recordingNumber;
        evm.eventID = id;
        evm.nodeID = processor;
        String str = String((char*)data);
        str.copyToUTF8(evm.text, MAX_STR_SIZE);
        evptr = (void*)&evm;     
    }
    else if (eventNames[type] == "TTL")
    {
        evt.time = time;
        evt.recording = recordingNumber;
        evt.eventID = id;
        evt.nodeID = processor;
        evt.event_channel = *((uint8*)data);
        evptr = (void*)&evt;
    }    
    //Perhaps this could work? Particularly useful if there are more different event types, otherwise whatever.
    //fill a buffer that pretends to be a struct for CompType
//    HeapBlock<char> hb(eventSizes[type]);
//    char* buf = hb.getData();
//    int pos = 0;
//    memcpy(buf, (char*)&timestamp, sizeof(uint64));
//    pos += sizeof(uint64);
//    memcpy(buf+pos, (char*)&recordingNumber, sizeof(int));
//    pos += sizeof(int);
//    memcpy(buf+pos, (char*)&id, sizeof(uint8));
//    pos += sizeof(uint8);
//    memcpy(buf+pos, (char*)&processor, sizeof(uint8));
//    pos += sizeof(uint8);
//    int datasize;
//    //strings that are passed can be of variable length, all other types are fixed length
//    datasize = (eventTypes[type] == STR) ? strlen((char*)data) : getNativeType(eventTypes[type]).getSize();
//    memcpy(buf+pos, (char*)data, datasize);
    eventFullData[type]->writeCompoundData(1, 0, eventCompTypes[type], evptr);
//    hb.free();
    
    
    
    
}


void AEFile::addEventType(String name, DataTypes type, String dataName)
{    
    eventNames.add(name);
    eventTypes.add(type);
    eventDataNames.add(dataName);  
    
    size_t typesize = (type == STR) ? MAX_STR_SIZE : getNativeType(type).getSize();
    size_t size = 2*sizeof(uint8)+sizeof(int) + sizeof(float) + typesize;
    eventSizes.add(size);
    CompType ctype(size);
    if (name == "Messages")
    {         
        ctype.insertMember(H5std_string("start"), HOFFSET(MessageEvent, time), PredType::NATIVE_FLOAT);
        ctype.insertMember(H5std_string("recording"), HOFFSET(MessageEvent, recording), PredType::NATIVE_INT32);
        ctype.insertMember(H5std_string("eventID"), HOFFSET(MessageEvent, eventID), PredType::NATIVE_UINT8);
        ctype.insertMember(H5std_string("nodeID"), HOFFSET(MessageEvent, nodeID), PredType::NATIVE_UINT8);
        ctype.insertMember(H5std_string("Text"), HOFFSET(MessageEvent, text), getNativeType(type));        
        eventCompTypes.add(ctype);
    }
    else if (name == "TTL")
    {
        ctype.insertMember(H5std_string("start"), HOFFSET(TTLEvent, time), PredType::NATIVE_FLOAT);
        ctype.insertMember(H5std_string("recording"), HOFFSET(TTLEvent, recording), PredType::NATIVE_INT32);
        ctype.insertMember(H5std_string("eventID"), HOFFSET(TTLEvent, eventID), PredType::NATIVE_UINT8);
        ctype.insertMember(H5std_string("nodeID"), HOFFSET(TTLEvent, nodeID), PredType::NATIVE_UINT8);
        ctype.insertMember(H5std_string("event_channel"), HOFFSET(TTLEvent, event_channel), getNativeType(type));
        eventCompTypes.add(ctype);      
    }
    else
    {
        std::cerr << "Event type" << name << "not implemented; look at ArfFileFormat.cpp";
    }

//Again, this is the nice idea for event types. Look at writeEvent.
//    eventCompTypes.add(ctype);
//
//    size_t offset = 0;
//    ctype.insertMember(H5std_string("timestamp"), offset, PredType::NATIVE_UINT64);
//    offset += sizeof(uint64);
//    ctype.insertMember(H5std_string("recording"), offset, PredType::NATIVE_INT32);
//    offset += sizeof(int);
//    ctype.insertMember(H5std_string("eventID"), offset, PredType::NATIVE_UINT8);
//    offset += sizeof(uint8);
//    ctype.insertMember(H5std_string("nodeID"), offset, PredType::NATIVE_UINT8);
//    offset += sizeof(uint8);
//    ctype.insertMember(H5std_string(dataName.toUTF8()), offset, getNativeType(type));
//    eventCompTypes.add(ctype);
}

//Spikes file

AXFile::AXFile(String basename) : ArfFileBase()
{
    initFile(basename);
    numElectrodes=0;
    transformVector.malloc(MAX_TRANSFORM_SIZE);
}

AXFile::AXFile() : ArfFileBase()
{
    numElectrodes=0;
    transformVector.malloc(MAX_TRANSFORM_SIZE);
}

AXFile::~AXFile()
{
}

String AXFile::getFileName()
{
    return filename;
}

void AXFile::initFile(String basename)
{
    if (isOpen()) return;
    filename = basename + "_spikes.arf";
    readyToOpen=true;
}

int AXFile::createFileStructure()
{
    const uint16 ver = 2;
    if (createGroup("/channel_groups")) return -1;
    String uuid = Uuid().toDashedString();
    CHECK_ERROR(setAttributeStr(uuid, String("/channel_groups"), String("uuid")));
    if (setAttributeStr(ARF_VERSION, "/", "arf_version")) return -1;
    for (int i=0; i < channelArray.size(); i++)
    {
        int res = createChannelGroup(i);
        if (res) return -1;
    }
    return 0;
}

void AXFile::addChannelGroup(int nChannels)
{
    channelArray.add(nChannels);
    numElectrodes++;
    
    //Create the compound datatype for that group
    CompType spiketype(sizeof(SpikeInfo));
    hsize_t dims[2] = {MAX_TRANSFORM_SIZE/(uint64)nChannels, (uint64)nChannels};
    spiketype.insertMember(H5std_string("waveform"), HOFFSET(SpikeInfo, waveform), ArrayType(getNativeType(I16), 2, dims));
    spiketype.insertMember(H5std_string("recording"), HOFFSET(SpikeInfo, recording), getNativeType(U16));
    spiketype.insertMember(H5std_string("start"), HOFFSET(SpikeInfo, time), PredType::NATIVE_FLOAT);
    spiketype.insertMember(H5std_string("valid_samples"), HOFFSET(SpikeInfo, samples), getNativeType(I32));
    spikeCompTypes.add(spiketype);
}

int AXFile::createChannelGroup(int index)
{
    ScopedPointer<ArfRecordingData> dSet;
    int nChannels = channelArray[index];
    String path("/channel_groups/"+String(index));
    //CHECK_ERROR(createGroup(path));
    
    int max_dims[3] = {0, 0, 0}; //first dimension set to 0, because we want it unlimited
    int chunk_dims[3] = {SPIKE_CHUNK_XSIZE, 0, 0};
    dSet = createCompoundDataSet(spikeCompTypes[index], path, 1, max_dims, chunk_dims);
    CHECK_ERROR(setAttributeStr(String("samples"), path, String("units")));
    
    return 0;
}

void AXFile::startNewRecording(int recordingNumber)
{
    ArfRecordingData* dSet;
    String path;
    this->recordingNumber = recordingNumber;

    for (int i=0; i < channelArray.size(); i++)
    {
        path = "/channel_groups/"+String(i);
        
        dSet = getDataSet(path);
        spikeFullDataArray.add(dSet);
    }
}

void AXFile::stopRecording()
{

    spikeFullDataArray.clear();
    
}

void AXFile::resetChannels()
{
    stopRecording(); //Just in case
    channelArray.clear();
}

void AXFile::writeSpike(int groupIndex, int nSamples, const uint16* data, float time)
{
    if ((groupIndex < 0) || (groupIndex >= numElectrodes))
    {
        std::cerr << "HDF5::writeSpike Electrode index out of bounds " << groupIndex << std::endl;
        return;
    }
    int nChans= channelArray[groupIndex];
    
    if (nSamples > MAX_TRANSFORM_SIZE)
    {
        std::cerr << "Spike nSamples is bigger than MAX_TRANSFORM_SIZE/nChannels in group" << groupIndex << std::endl;
    }
    
    spikeinfo.recording = recordingNumber;
    spikeinfo.time = time;
    spikeinfo.samples = nSamples;
    
    int16* dst=transformVector;
    int16* waveBuf = spikeinfo.waveform;

    //Given the way we store spike data, we need to transpose it to store in
    //NSAMPLES x NCHANNELS as well as convert from u16 to i16
    for (int i = 0; i < nSamples; i++)
    {
        for (int j = 0; j < nChans; j++)
        {
            *(dst++) = *(data+j*nSamples+i)-32768;
            *(waveBuf++) = *(data+j*nSamples+i)-32768;
        }
    }
    //Fill the rest of buffer with 0s, so that we don't write any junk. 
    //This is annoying, but it seems there are problems trying to do Variable Length types in Compound Types
    while(waveBuf < spikeinfo.waveform+MAX_TRANSFORM_SIZE)
        {*(waveBuf++) = 0;}
    
    spikeFullDataArray[groupIndex]->writeCompoundData(1, 0, spikeCompTypes[groupIndex], (void*)&spikeinfo);

}
