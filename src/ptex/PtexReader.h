#ifndef PtexReader_h
#define PtexReader_h

/* 
PTEX SOFTWARE
Copyright 2009 Disney Enterprises, Inc.  All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
    Studios" or the names of its contributors may NOT be used to
    endorse or promote products derived from this software without
    specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/
#include <stdio.h>
#include <zlib.h>
#include <vector>
#include <string>
#include <map>
#include <errno.h>
#include "Ptexture.h"
#include "PtexIO.h"
#include "PtexUtils.h"

#include "PtexHashMap.h"
using namespace PtexInternal;

class PtexReader : public PtexTexture, public PtexIO {
public:
    PtexReader(bool premultiply, PtexInputHandler* handler);
    virtual ~PtexReader();
    virtual void release() { delete this; }
    bool needToOpen() const { return _needToOpen; }
    bool open(const char* path, Ptex::String& error);
    void prune();
    void purge();
    bool tryClose();
    bool ok() const { return _ok; }
    bool isOpen() { return _fp; }
    size_t memUsed() { return _memUsed; }
    void increaseMemUsed(size_t amount) { if (amount) AtomicAdd(&_memUsed, amount); }

    virtual const char* path() { return _path.c_str(); }
    virtual Ptex::MeshType meshType() { return MeshType(_header.meshtype); }
    virtual Ptex::DataType dataType() { return DataType(_header.datatype); }
    virtual Ptex::BorderMode uBorderMode() { return BorderMode(_extheader.ubordermode); }
    virtual Ptex::BorderMode vBorderMode() { return BorderMode(_extheader.vbordermode); }
    virtual Ptex::EdgeFilterMode edgeFilterMode() { return EdgeFilterMode(_extheader.edgefiltermode); }
    virtual int alphaChannel() { return _header.alphachan; }
    virtual int numChannels() { return _header.nchannels; }
    virtual int numFaces() { return _header.nfaces; }
    virtual bool hasEdits() { return _hasEdits; }
    virtual bool hasMipMaps() { return _header.nlevels > 1; }

    virtual PtexMetaData* getMetaData();
    virtual const Ptex::FaceInfo& getFaceInfo(int faceid);
    virtual void getData(int faceid, void* buffer, int stride);
    virtual void getData(int faceid, void* buffer, int stride, Res res);
    virtual PtexFaceData* getData(int faceid);
    virtual PtexFaceData* getData(int faceid, Res res);
    virtual void getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels);
    virtual void getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels,
			  Ptex::Res res);

    DataType datatype() const { return _header.datatype; }
    int nchannels() const { return _header.nchannels; }
    int pixelsize() const { return _pixelsize; }
    const Header& header() const { return _header; }
    const ExtHeader& extheader() const { return _extheader; }
    const LevelInfo& levelinfo(int level) const { return _levelinfo[level]; }

    class MetaData : public PtexMetaData {
    public:
	MetaData(PtexReader* reader)
	    : _reader(reader) {}
        ~MetaData() {
            for (std::vector<Entry*>::iterator i = _entries.begin(); i != _entries.end(); ++i) {
                (*i)->clear();
            }
        }
	virtual void release() {
	}

 	virtual int numKeys() { return int(_entries.size()); }
	virtual void getKey(int n, const char*& key, MetaDataType& type)
	{
	    Entry* e = _entries[n];
	    key = e->key;
	    type = e->type;
	}

	virtual void getValue(const char* key, const char*& value)
	{
	    Entry* e = getEntry(key);
	    if (e) value = (const char*) e->data;
	    else value = 0;
	}

	virtual void getValue(const char* key, const int8_t*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) { value = (const int8_t*) e->data; count = e->datasize; }
	    else { value = 0; count = 0; }
	}

	virtual void getValue(const char* key, const int16_t*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const int16_t*) e->data;
		count = int(e->datasize/sizeof(int16_t));
	    }
	    else { value = 0; count = 0; }
	}

	virtual void getValue(const char* key, const int32_t*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const int32_t*) e->data;
		count = int(e->datasize/sizeof(int32_t));
	    }
	    else { value = 0; count = 0; }
	}

	virtual void getValue(const char* key, const float*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const float*) e->data;
		count = int(e->datasize/sizeof(float));
	    }
	    else { value = 0; count = 0; }
	}

	virtual void getValue(const char* key, const double*& value, int& count)
	{
	    Entry* e = getEntry(key);
	    if (e) {
		value = (const double*) e->data;
		count = int(e->datasize/sizeof(double));
	    }
	    else { value = 0; count = 0; }
	}

	void addEntry(uint8_t keysize, const char* key, uint8_t datatype,
		      uint32_t datasize, void* data, size_t& metaDataMemUsed)
	{
	    Entry* e = newEntry(keysize, key, datatype, datasize);
	    e->data = malloc(datasize);
	    memcpy(e->data, data, datasize);
            metaDataMemUsed += sizeof(std::string) + keysize + 1 + sizeof(Entry) + datasize;
	}

	void addLmdEntry(uint8_t keysize, const char* key, uint8_t datatype,
			 uint32_t datasize, FilePos filepos, uint32_t zipsize,
                         size_t& metaDataMemUsed)
	{
	    Entry* e = newEntry(keysize, key, datatype, datasize);
	    e->isLmd = true;
	    e->lmdData = 0;
	    e->lmdPos = filepos;
	    e->lmdZipSize = zipsize;
            metaDataMemUsed += sizeof(Entry);
	}

        size_t selfDataSize()
        {
            return sizeof(*this) + sizeof(Entry*) * _entries.capacity();
        }

    protected:
	class LargeMetaData
	{
	 public:
	    LargeMetaData(int size)
		: _data(malloc(size)) {}
	    virtual ~LargeMetaData() { free(_data); }
	    void* data() { return _data; }
        private:
	    void* _data;
	};

	struct Entry {
	    const char* key;	      // ptr to map key string
	    MetaDataType type;	      // meta data type
	    uint32_t datasize;	      // size of data in bytes
	    void* data;		      // if lmd, data only valid when lmd is loaded and ref'ed
	    bool isLmd;		      // true if data is a large meta data block
	    LargeMetaData* lmdData;   // large meta data (lazy-loaded)
	    FilePos lmdPos;	      // large meta data file position
	    uint32_t lmdZipSize;      // large meta data size on disk

	    Entry() :
		key(0), type(MetaDataType(0)), datasize(0), data(0),
		isLmd(0), lmdData(0), lmdPos(0), lmdZipSize(0) {}
	    ~Entry() { clear(); }
	    void clear() {
		if (isLmd) {
		    isLmd = 0;
		    if (lmdData) { delete lmdData; lmdData = 0; }
		    lmdPos = 0;
		    lmdZipSize = 0;
		}
		else {
		    free(data);
		}
		data = 0;
	    }
	};

	Entry* newEntry(uint8_t keysize, const char* key, uint8_t datatype, uint32_t datasize)
	{
	    std::pair<MetaMap::iterator,bool> result =
		_map.insert(std::make_pair(std::string(key, keysize), Entry()));
	    Entry* e = &result.first->second;
	    bool newEntry = result.second;
	    if (newEntry) _entries.push_back(e);
	    else e->clear();
	    e->key = result.first->first.c_str();
	    e->type = MetaDataType(datatype);
	    e->datasize = datasize;
	    return e;
	}

	Entry* getEntry(const char* key);

	PtexReader* _reader;
	typedef std::map<std::string, Entry> MetaMap;
	MetaMap _map;
	std::vector<Entry*> _entries;
    };


    class ConstDataPtr : public PtexFaceData {
    public:
	ConstDataPtr(void* data, int pixelsize)
	    : _data(data), _pixelsize(pixelsize) {}
	virtual void release() { delete this; }
	virtual Ptex::Res res() { return 0; }
	virtual bool isConstant() { return true; }
	virtual void getPixel(int, int, void* result)
	{ memcpy(result, _data, _pixelsize); }
	virtual void* getData() { return _data; }
	virtual bool isTiled() { return false; }
	virtual Ptex::Res tileRes() { return 0; }
	virtual PtexFaceData* getTile(int) { return 0; }

    protected:
	void* _data;
	int _pixelsize;
    };


    class FaceData : public PtexFaceData {
    public:
	FaceData(Res res)
	    : _res(res) {}
        virtual ~FaceData() {}
	virtual void release() { }
	virtual Ptex::Res res() { return _res; }
	virtual FaceData* reduce(PtexReader*, Res newres, PtexUtils::ReduceFn, size_t& newMemUsed) = 0;
    protected:
	Res _res;
    };

    class PackedFace : public FaceData {
    public:
	PackedFace(Res res, int pixelsize, int size)
	    : FaceData(res),
	      _pixelsize(pixelsize), _data(malloc(size)) {}
	void* data() { return _data; }
	virtual bool isConstant() { return false; }
	virtual void getPixel(int u, int v, void* result)
	{
	    memcpy(result, (char*)_data + (v*_res.u() + u) * _pixelsize, _pixelsize);
	}
	virtual void* getData() { return _data; }
	virtual bool isTiled() { return false; }
	virtual Ptex::Res tileRes() { return _res; }
	virtual PtexFaceData* getTile(int) { return 0; }
	virtual FaceData* reduce(PtexReader*, Res newres, PtexUtils::ReduceFn, size_t& newMemUsed);

    protected:
	virtual ~PackedFace() { free(_data); }

	int _pixelsize;
	void* _data;
    };

    class ConstantFace : public PackedFace {
    public:
	ConstantFace(int pixelsize)
	    : PackedFace(0, pixelsize, pixelsize) {}
	virtual bool isConstant() { return true; }
	virtual void getPixel(int, int, void* result) { memcpy(result, _data, _pixelsize); }
	virtual FaceData* reduce(PtexReader*,
			    Res newres, PtexUtils::ReduceFn);
    };


    class TiledFaceBase : public FaceData {
    public:
	TiledFaceBase(PtexReader* reader, Res res, Res tileres)
	    : FaceData(res),
              _reader(reader),
	      _tileres(tileres)
	{
            _dt = reader->datatype();
            _nchan = reader->nchannels();
            _pixelsize = DataSize(_dt)*_nchan;
	    _ntilesu = _res.ntilesu(tileres);
	    _ntilesv = _res.ntilesv(tileres);
	    _ntiles = _ntilesu*_ntilesv;
	    _tiles.resize(_ntiles);
	}

	virtual void release() { }
	virtual bool isConstant() { return false; }
	virtual void getPixel(int u, int v, void* result);
	virtual void* getData() { return 0; }
	virtual bool isTiled() { return true; }
	virtual Ptex::Res tileRes() { return _tileres; }
	virtual FaceData* reduce(PtexReader*, Res newres, PtexUtils::ReduceFn, size_t& newMemUsed);
	Res tileres() const { return _tileres; }
	int ntilesu() const { return _ntilesu; }
	int ntilesv() const { return _ntilesv; }
	int ntiles() const { return _ntiles; }

    protected:
        size_t baseExtraMemUsed() { return _tiles.size() * sizeof(_tiles[0]); }

	virtual ~TiledFaceBase() {
            for (std::vector<FaceData*>::iterator i = _tiles.begin(); i != _tiles.end(); ++i) {
                if (*i) delete *i;
            }
        }

	PtexReader* _reader;
	Res _tileres;
	DataType _dt;
	int _nchan;
	int _ntilesu;
	int _ntilesv;
	int _ntiles;
	int _pixelsize;
	std::vector<FaceData*> _tiles;
    };


    class TiledFace : public TiledFaceBase {
    public:
	TiledFace(PtexReader* reader, Res res, Res tileres, int levelid)
	    : TiledFaceBase(reader, res, tileres),
	      _levelid(levelid)
	{
	    _fdh.resize(_ntiles),
	    _offsets.resize(_ntiles);
	}
	virtual PtexFaceData* getTile(int tile)
	{
	    FaceData*& f = _tiles[tile];
	    if (!f) readTile(tile, f);
	    return f;
	}
	void readTile(int tile, FaceData*& data);
        size_t memUsed() {
            return sizeof(*this) + baseExtraMemUsed() + _fdh.size() * (sizeof(_fdh[0]) + sizeof(_offsets[0]));
        }

    protected:
	friend class PtexReader;
	int _levelid;
	std::vector<FaceDataHeader> _fdh;
	std::vector<FilePos> _offsets;
    };


    class TiledReducedFace : public TiledFaceBase {
    public:
	TiledReducedFace(PtexReader* reader, Res res, Res tileres,
                         TiledFaceBase* parentface, PtexUtils::ReduceFn reducefn)
	    : TiledFaceBase(reader, res, tileres),
	      _parentface(parentface),
	      _reducefn(reducefn)
	{
	}
	~TiledReducedFace()
	{
	}
	virtual PtexFaceData* getTile(int tile);

        size_t memUsed() { return sizeof(*this) + baseExtraMemUsed(); }

    protected:
	TiledFaceBase* _parentface;
	PtexUtils::ReduceFn* _reducefn;
    };


    class Level {
    public:
	std::vector<FaceDataHeader> fdh;
	std::vector<FilePos> offsets;
	std::vector<FaceData*> faces;

	Level(int nfaces)
	    : fdh(nfaces),
	      offsets(nfaces),
	      faces(nfaces) {}

	~Level() {
            for (std::vector<FaceData*>::iterator i = faces.begin(); i != faces.end(); ++i) {
                if (*i) delete *i;
            }
        }

        size_t memUsed() {
            return sizeof(*this) + fdh.size() * (sizeof(fdh[0]) +
                                                 sizeof(offsets[0]) +
                                                 sizeof(faces[0]));
        }
    };

protected:
    virtual void logOpen() {}
    virtual void logBlockRead() {}

    void setError(const char* error)
    {
	_error = error; _error += " PtexFile: "; _error += _path;
	_ok = 0;
    }

    FilePos tell() { return _pos; }
    void seek(FilePos pos)
    {
        if (!_fp && !reopenFP()) return;
        logBlockRead();
	if (pos != _pos) {
	    _io->seek(_fp, pos);
	    _pos = pos;
	}
    }

    void closeFP();
    bool reopenFP();
    bool readBlock(void* data, int size, bool reportError=true);
    bool readZipBlock(void* data, int zipsize, int unzipsize);
    Level* getLevel(int levelid)
    {
	Level*& level = _levels[levelid];
	if (!level) readLevel(levelid, level);
	return level;
    }

    uint8_t* getConstData() { if (!_constdata) readConstData(); return _constdata; }
    FaceData* getFace(int levelid, Level* level, int faceid, Res res)
    {
	FaceData*& face = level->faces[faceid];
	if (!face) readFace(levelid, level, faceid, res);
	return face;
    }

    void readFaceInfo();
    void readLevelInfo();
    void readConstData();
    void readLevel(int levelid, Level*& level);
    void readFace(int levelid, Level* level, int faceid, Res res);
    void readFaceData(FilePos pos, FaceDataHeader fdh, Res res, int levelid, FaceData*& face);
    void readMetaData();
    void readMetaDataBlock(MetaData* metadata, FilePos pos, int zipsize, int memsize, size_t& metaDataMemUsed);
    void readLargeMetaDataHeaders(MetaData* metadata, FilePos pos, int zipsize, int memsize, size_t& metaDataMemUsed);
    void readEditData();
    void readEditFaceData();
    void readEditMetaData();

    void computeOffsets(FilePos pos, int noffsets, const FaceDataHeader* fdh, FilePos* offsets)
    {
	FilePos* end = offsets + noffsets;
	while (offsets != end) { *offsets++ = pos; pos += fdh->blocksize(); fdh++; }
    }

    class DefaultInputHandler : public PtexInputHandler
    {
        char* buffer;
     public:
        DefaultInputHandler() : buffer(0) {}
        virtual Handle open(const char* path) {
            FILE* fp = fopen(path, "rb");
            if (fp) {
                buffer = (char*) malloc(IBuffSize);
                setvbuf(fp, buffer, _IOFBF, IBuffSize);
            }
            else buffer = 0;
            return (Handle) fp;
        }
        virtual void seek(Handle handle, int64_t pos) { fseeko((FILE*)handle, pos, SEEK_SET); }
        virtual size_t read(void* buffer, size_t size, Handle handle) {
            return fread(buffer, size, 1, (FILE*)handle) == 1 ? size : 0;
        }
        virtual bool close(Handle handle) {
            bool ok = handle && (fclose((FILE*)handle) == 0);
            if (buffer) free(buffer);
            buffer = 0;
            return ok;
        }
        virtual const char* lastError() { return strerror(errno); }
    };

    Mutex readlock;
    DefaultInputHandler _defaultIo;   // Default IO handler
    PtexInputHandler* _io;	      // IO handler
    bool _premultiply;		      // true if reader should premultiply the alpha chan
    bool _ok;			      // flag set if read error occurred)
    bool _needToOpen;                 // true if file needs to be opened (or reopened after a purge)
    std::string _error;		      // error string (if !_ok)
    PtexInputHandler::Handle _fp;     // file pointer
    FilePos _pos;		      // current seek position
    std::string _path;		      // current file path
    Header _header;		      // the header
    ExtHeader _extheader;	      // extended header
    FilePos _faceinfopos;	      // file positions of data sections
    FilePos _constdatapos;            // ...
    FilePos _levelinfopos;
    FilePos _leveldatapos;
    FilePos _metadatapos;
    FilePos _lmdheaderpos;
    FilePos _lmddatapos;
    FilePos _editdatapos;
    int _pixelsize;		      // size of a pixel in bytes
    uint8_t* _constdata;	      // constant pixel value per face
    MetaData* _metadata;	      // meta data (read on demand)
    bool _hasEdits;		      // has edit blocks

    std::vector<FaceInfo> _faceinfo;   // per-face header info
    std::vector<uint32_t> _rfaceids;   // faceids sorted in reduction order
    std::vector<LevelInfo> _levelinfo; // per-level header info
    std::vector<FilePos> _levelpos;    // file position of each level's data
    std::vector<Level*> _levels;	      // level data (read on demand)

    struct MetaEdit
    {
	FilePos pos;
	int zipsize;
	int memsize;
    };
    std::vector<MetaEdit> _metaedits;

    struct FaceEdit
    {
	FilePos pos;
	int faceid;
	FaceDataHeader fdh;
    };
    std::vector<FaceEdit> _faceedits;

    class ReductionKey {
        int64_t _val;
    public:
	ReductionKey() : _val(-1) {}
	ReductionKey(uint32_t faceid, Res res)
            : _val( int64_t(faceid)<<32 | uint32_t(16777619*((res.val()<<16) ^ faceid)) ) {}

        void copy(volatile ReductionKey& key) volatile
        {
            _val = key._val;
        }

        void move(volatile ReductionKey& key) volatile
        {
            _val = key._val;
        }

        bool matches(const ReductionKey& key) volatile
	{
            return _val == key._val;
        }
        bool isEmpty() volatile { return _val==-1; }
        uint32_t hash() volatile
        {
            return uint32_t(_val);
        }
    };
    typedef PtexHashMap<ReductionKey, FaceData*> ReductionMap;
    ReductionMap _reductions;

    z_stream_s _zstream;
    size_t _baseMemUsed;
    size_t _memUsed;
};

#endif
