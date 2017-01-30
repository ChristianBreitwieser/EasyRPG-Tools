/*
 * Copyright (c) 2014 Lcf2Xml authors
 * This file is released under the MIT License
 * http://opensource.org/licenses/MIT
 */

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "ldb_reader.h"
#include "lmt_reader.h"
#include "lmu_reader.h"
#include "lsd_reader.h"
#include "reader_util.h"

#ifndef _WIN32
#include <dirent.h>
#endif

enum FileTypes
{
	FileType_LCF_MapUnit,
	FileType_LCF_SaveData,
	FileType_LCF_Database,
	FileType_LCF_MapTree,
	FileType_XML_MapUnit,
	FileType_XML_SaveData,
	FileType_XML_Database,
	FileType_XML_MapTree,
	FileType_Invalid
};

std::string GetFilename(const std::string& str);
FileTypes GetFiletype(const std::string& in_file, std::string& out_extension);
void PrintReaderError(const std::string data);
int ReaderWriteToFile(const std::string& in_filename, std::istream& in, std::ostream& out, FileTypes in_type);

int main(int argc, char** argv)
{
	if (argc <= 1)
	{
		std::cerr << "Lcf2Xml - Converts RPG Maker 2k/2k3 Files to XML and vice versa" << std::endl;
		std::cerr << "Usage: lcf2xml [FILE] [OUTFILE]" << std::endl;
		//std::cerr << "If OUTFILE is missing the resulting file is named FILE with the extension replaced" << std::endl;
		return 1;
	}

	std::string infile;
	std::string outfile;

	infile = argv[1];

	FileTypes type;
	std::string extension;

	if (argc >= 3)
	{
		// OUTFILE
		outfile = argv[2];
		type = GetFiletype(infile, extension);
	}
	else
	{
		// No OUTFILE, add extension based on detected filetype
		outfile = GetFilename(infile);
		type = GetFiletype(infile, extension);
		outfile += extension;
	}
	
	int inmode = std::ios::ios_base::in;
	int outmode = std::ios::ios_base::out;

	switch (type) {
		case FileType_LCF_MapUnit:
		case FileType_LCF_SaveData:
		case FileType_LCF_Database:
		case FileType_LCF_MapTree:
			inmode |= std::ios::ios_base::binary;
			break;
		case FileType_XML_MapUnit:
		case FileType_XML_SaveData:
		case FileType_XML_Database:
		case FileType_XML_MapTree:
			outmode |= std::ios::ios_base::binary;
			break;
		case FileType_Invalid:
		default:
			//unsupported
			break;
	}

	std::ifstream instream(infile, inmode);
	std::ofstream outstream(outfile, outmode);

	return ReaderWriteToFile(infile, instream, outstream, type);

	instream.close();
	outstream.close();
}


/** Returns the filename (without extension). */
std::string GetFilename(const std::string& str)
{
	std::string s = str;
#ifdef _WIN32
	std::replace(s.begin(), s.end(), '\\', '/');
#endif
	// Extension
	size_t found = s.find_last_of(".");
	if (found != std::string::npos)
	{
		s = s.substr(0, found);
	}

	// Filename
	found = s.find_last_of("/");
	if (found == std::string::npos)
	{
		return s;
	}

	s = s.substr(found + 1);
	return s;
}

/** Returns that path (everything left to the last /) */
std::string GetPath(const std::string& str) {
	std::string s = str;
#ifdef _WIN32
	std::replace(s.begin(), s.end(), '\\', '/');
#endif

	// Path
	size_t found = s.find_last_of("/");
	if (found == std::string::npos)
	{
		return ".";
	}

	s = s.substr(0, found);
	return s;
}

/** Uses heuristics to detect the file type. */
FileTypes GetFiletype(const std::string& in_file, std::string& out_extension)
{
	std::ifstream in(in_file.c_str());

	char buf[128];
	memset(buf, '\0', 128);

	in.seekg(1, std::ios::beg);
	in.read(buf, 10);
	std::string input(buf);

	out_extension = ".xml";
	if (input == "LcfDataBas") {
		return FileType_LCF_Database;
	} else if (input == "LcfMapTree") {
		return FileType_LCF_MapTree;
	} else if (input == "LcfSaveDat") {
		return FileType_LCF_SaveData;
	} else if (input == "LcfMapUnit") {
		return FileType_LCF_MapUnit;
	} else if (input == "?xml versi") {
		in.read(buf, 128);
		std::string in(buf);

		size_t pos = in.find('<');
		if (pos != std::string::npos)
		{
			in = in.substr(pos + 1, 3);
			if (in == "LDB") {
				out_extension = ".ldb";
				return FileType_XML_Database;
			} else if (in == "LMT") {
				out_extension = ".lmt";
				return FileType_XML_MapTree;
			} else if (in == "LSD") {
				out_extension = ".lsd";
				return FileType_XML_SaveData;
			} else if (in == "LMU") {
				out_extension = ".lmu";
				return FileType_XML_MapUnit;
			}
		}
	}

	out_extension = "";
	return FileType_Invalid;
}

/** Utility func for errors. */
void PrintReaderError(const std::string data)
{
	std::cerr << data << " error: " << LcfReader::GetError() << std::endl;
}

#define LCFXML_ERROR(cond, text) \
	if (cond) {\
		PrintReaderError(text);\
		return 2;\
	}

/** Takes data from in and writes converted data into out using liblcf. */
int ReaderWriteToFile(const std::string& in_filename, std::istream& in, std::ostream& out, FileTypes in_type)
{
	std::string path = GetPath(in_filename) + "/";
	std::string encoding = "";

#ifdef _WIN32
	std::ifstream inistream(path + "RPG_RT.ini", std::ios::ios_base::in);
	encoding = ReaderUtil::GetEncoding(inistream);
#else
	DIR* dir = opendir(path.c_str());
	if (dir) {
		struct dirent* ent;
		while ((ent = ::readdir(dir)) != NULL) {
			if (ent->d_name[0] == '.') { continue; }
			std::string name = ent->d_name;

			std::transform(name.begin(), name.end(), name.begin(), ::tolower);

			if (name == "rpg_rt.ini") {
				std::ifstream inistream(path + ent->d_name, std::ios::ios_base::in);
				encoding = ReaderUtil::GetEncoding(inistream);
				closedir(dir);
				goto dirsuccess;
				break;
			}
		}
		closedir(dir);
	}
	else {
		std::cerr << "Failed opening directory " << path << std::endl;
	}

	dirsuccess:
#endif

	switch (in_type)
	{
		case FileType_LCF_MapUnit:
		{
			std::unique_ptr<RPG::Map> file = LMU_Reader::Load(in, encoding);
			LCFXML_ERROR(file.get() == NULL, "LMU load");
			LCFXML_ERROR(!LMU_Reader::SaveXml(out, *file), "LMU XML save");
			break;
		}
		case FileType_LCF_SaveData:
		{
			std::unique_ptr<RPG::Save> file = LSD_Reader::Load(in, encoding);
			LCFXML_ERROR(file.get() == NULL, "LSD load");
			LCFXML_ERROR(!LSD_Reader::SaveXml(out, *file), "LSD XML save");
			break;
		}
		case FileType_LCF_Database:
		{
			LCFXML_ERROR(!LDB_Reader::Load(in, encoding), "LDB load");
			LCFXML_ERROR(!LDB_Reader::SaveXml(out), "LDB XML save");
			break;
		}
		case FileType_LCF_MapTree:
		{
			LCFXML_ERROR(!LMT_Reader::Load(in, encoding), "LMT load");
			LCFXML_ERROR(!LMT_Reader::SaveXml(out), "LMT XML save");
			break;
		}
		case FileType_XML_MapUnit:
		{
			std::unique_ptr<RPG::Map> file = LMU_Reader::LoadXml(in);
			LCFXML_ERROR(file.get() == NULL, "LMU XML load");
			LCFXML_ERROR(!LMU_Reader::Save(out, *file, encoding), "LMU save");
			break;
		}
		case FileType_XML_SaveData:
		{
			std::unique_ptr<RPG::Save> file = LSD_Reader::LoadXml(in);
			LCFXML_ERROR(file.get() == NULL, "LSD XML load");
			LCFXML_ERROR(!LSD_Reader::Save(out, *file, encoding), "LSD save");
			break;
		}
		case FileType_XML_Database:
		{
			LCFXML_ERROR(!LDB_Reader::LoadXml(in), "LDB XML load");
			LCFXML_ERROR(!LDB_Reader::Save(out, encoding), "LDB save");
			break;
		}
		case FileType_XML_MapTree:
		{
			LCFXML_ERROR(!LMT_Reader::LoadXml(in), "LMT XML load");
			LCFXML_ERROR(!LMT_Reader::Save(out, encoding), "LMT save");
			break;
		}
		case FileType_Invalid:
		{
			std::cerr << in_filename << " unsupported" << std::endl;
			return 2;
		}
	}

	return 0;
}
