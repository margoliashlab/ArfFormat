/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2013 Open Ephys

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

#ifndef ARFFILESOURCE_H_INCLUDED
#define ARFFILESOURCE_H_INCLUDED

#include <FileSourceHeaders.h>

#define MIN_ARF_VERSION 2
#define MAX_ARF_VERSION 2
//TODO version?

class ArfRecordingData;
namespace H5
{
class DataSet;
class H5File;
class DataType;
}

class ArfFileSource : public FileSource
{
public:
    ArfFileSource();
    ~ArfFileSource();

    int readData(int16* buffer, int nSamples) override;

    void seekTo(int64 sample) override;

    void processChannelData(int16* inBuffer, float* outBuffer, int channel, int64 numSamples) override;

	bool isReady() override;

private:
    ScopedPointer<H5::H5File> sourceFile;
    ScopedPointer<H5::DataSet> dataSet;
    bool Open(File file) override;
    void fillRecordInfo() override;
    void updateActiveRecord() override;
    int64 samplePos;
    Array<int> availableDataSets;
	bool skipRecordEngineCheck;
};



#endif  // ARFFILESOURCE_H_INCLUDED
