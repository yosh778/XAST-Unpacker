// XAST Unpacker by yosh778

#include <string>
#include <iostream>
#include <algorithm>

#include <sstream>
#include <fstream>
#include <cinttypes>
#include <boost/filesystem.hpp>

uint32_t read32(std::ifstream& input) {
	uint32_t data;
	input.read((char*)&data, (int)sizeof(data));
	return data;
}

uint64_t read64(std::ifstream& input) {
	uint64_t data;
	input.read((char*)&data, (int)sizeof(data));
	return data;
}

// Checksum reverse by weaknespase
uint32_t checksum(const char* in, const uint32_t length, int last = 0){
	const char* end = in + length;
	int acc = last;
	while (in < end)
		acc = (acc * 33) ^ (unsigned char) *in++;
	return acc;
}

int main(int argc, char *argv[])
{
	if ( argc < 3 ) {
		std::cerr << "Usage : " << argv[0] << " <input> <output>" << std::endl;
		return -1;
	}

	std::string iDir = argv[1];
	std::string oDir = argv[2];

	std::ifstream file( iDir.c_str(), std::ios_base::binary );

	if ( !file.is_open() )
		return -2;

	uint32_t data;

	// XAST header
	uint32_t magic = read32(file);

	// Version : 1.01
	uint32_t version = read32(file);
	uint32_t nbEntries = read32(file); // nbEntries
	uint32_t maxEntries = read32(file); // maxEntries
	uint32_t pathsCount = read32(file);
	uint32_t dataOffset = read32(file);
	uint32_t unk0 = read32(file);
	uint32_t headersCount = read32(file);
	uint32_t fileSize = read32(file);

	uint32_t pathsOffset = 0x30 + headersCount;

	file.seekg(0x30);

	uint64_t *fileSizes = new uint64_t[maxEntries];
	uint64_t *fileOffsets = new uint64_t[maxEntries];
	uint32_t *pathOffsets = new uint32_t[maxEntries];
	uint32_t *pathHash = new uint32_t[maxEntries];
	uint32_t *fileChecksums = new uint32_t[maxEntries];
	uint32_t nbActualEntries = 0;
	uint32_t i = 0;
	std::stringstream ss;

	for (i = 0; i < maxEntries; i++) {

		pathHash[i] = read32(file);
		pathOffsets[i] = read32(file);
		fileChecksums[i] = read32(file);
		uint32_t isXai = read32(file);

		ss << std::hex << pathHash[i] << " | " << pathOffsets[i]
			<< " | " << fileChecksums[i] << " | " << isXai << std::endl;

		if ( isXai > 1 ) {
			std::cerr << "WARNING : unsupported value found for isXai : "
				<< isXai << std::endl;
		}

		fileSizes[i] = read64(file);

		ss << fileSizes[i] << std::endl;

		if ( uint64_t padding = read64(file) ) {
			std::cerr << "WARNING : unsupported value found for padding : "
				<< padding << std::endl;
			// return -3;
		}

		fileOffsets[i] = read64(file);
		uint64_t aSize = read64(file);

		ss << fileOffsets[i] << " | " << aSize << std::endl << std::endl;

		if ( file.tellg() >= pathsOffset ) {
			// std::cout << "break" << std::endl;
			// std::cout << std::hex << file.tellg() << std::endl;
			break;
		}
	}

	nbActualEntries = i + 1;

	// std::cout << "Number of declared entries : " << maxEntries << std::endl;
	// std::cout << "Number of actual entries   : " << nbActualEntries << std::endl;
	// std::cout << "Number of used entries     : " << nbEntries << std::endl;

	char **fileNames = new char*[maxEntries];

	file.seekg(pathsOffset);

	char *pathsData = new char[pathsCount];
	file.read(pathsData, pathsCount);

	uint32_t n = 0;
	char *next = pathsData;
	char *end = pathsData + pathsCount;

	i = 0;

	while ( next < end ) {

		fileNames[n++] = next;

		ss << next << " | " << pathsOffset + i << std::endl;

		// Prints file paths
		// std::cout << next << std::endl;

		while ( pathsData[i++] && pathsData < end );

		next = pathsData + i;
	}

	uint8_t *buf = NULL;
	uint64_t bufSize = 0;
	uint32_t nFoundEntries = 0;

	for ( i = 0; i < maxEntries; i++ ) {

		uint64_t offset = fileOffsets[i];
		uint64_t size = fileSizes[i];

		if ( !offset || !size ) {
			std::cout << "Skipping empty slot" << std::endl;
			continue;
		}

		nFoundEntries++;

		file.seekg( offset );

		if ( size > bufSize ) {
			buf = (uint8_t*)realloc( buf, size );
			bufSize = size;
		}

		file.read( (char*)buf, size );

		std::string base = pathsData + (pathOffsets[i] - pathsOffset);
		std::string opath( oDir + "/" + base );
		std::cout << base << std::endl;

		uint32_t pathCheck = checksum( (char*)base.c_str(), base.size() );
		std::cout << "Computed path checksum : " << std::hex << pathCheck << std::dec << std::endl;
		std::cout << "Original path checksum : " << std::hex << pathHash[i] << std::dec << std::endl;

		uint32_t contentCheck = checksum( (char*)buf, size );
		std::cout << "Computed file checksum : " << std::hex << contentCheck << std::dec << std::endl;
		std::cout << "Original file checksum : " << std::hex << fileChecksums[i] << std::dec << std::endl;

		if ( contentCheck != fileChecksums[i] )
			std::cout << "File checksum error !" << std::endl;


		boost::system::error_code returnedError;
		boost::filesystem::path path( opath );
		//std::cout << path.parent_path() << std::endl;

		boost::filesystem::create_directories(
			path.parent_path(), returnedError
		);

		std::ofstream output( opath.c_str(), std::ios_base::binary );

		if ( !output.is_open() )
			continue;

		output.write( (char*)buf, size );
		output.close();
	}

	std::cerr <<
	std::endl << "Number of declared entries      : " << maxEntries << std::endl;
	std::cerr << "Number of actual entries        : " << nbActualEntries << std::endl;
	std::cerr << "Number of used entries          : " << nbEntries << std::endl;
	std::cerr << "Number of filepaths             : " << n << std::endl;
	std::cerr << "Number of extracted entries     : " << nFoundEntries << std::endl;

	file.close();

	return 0;
}

