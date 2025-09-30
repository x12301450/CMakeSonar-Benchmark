// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin developers
// Copyright (c) 2016-2020, The Karbo developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#include "DataBaseConfig.h"

#include <boost/utility/value_init.hpp>

#include <Common/Util.h>
#include "Common/CommandLine.h"
#include "Common/StringTools.h"
#include "crypto/crypto.h"
#include "CryptoNoteConfig.h"

using namespace CryptoNote;

namespace {

const uint64_t WRITE_BUFFER_MB_DEFAULT_SIZE     = 256; // Mb
const uint64_t READ_BUFFER_MB_DEFAULT_SIZE      = 128; // Mb
const uint32_t DEFAULT_MAX_OPEN_FILES           = 128; // Nr of files
const uint16_t DEFAULT_BACKGROUND_THREADS_COUNT = 8;   // DB threads
const uint64_t DEFAULT_MAX_BYTES_FOR_LEVEL_BASE = 512; // 512MB

const uint64_t LEVELDB_MAX_FILE_SIZE            = 1024; // 1GB

const uint64_t MEGABYTE = 1024 * 1024;

const command_line::arg_descriptor<uint16_t>    argBackgroundThreadsCount = { "db-threads", "Nuber of background threads used for compaction and flush", DEFAULT_BACKGROUND_THREADS_COUNT};
const command_line::arg_descriptor<uint32_t>    argMaxOpenFiles = { "db-max-open-files", "Number of open files that can be used by the DB", DEFAULT_MAX_OPEN_FILES};
const command_line::arg_descriptor<uint64_t>    argMaxFileSize = { "db-max-file-size", "Max file size that can be used by the DB", LEVELDB_MAX_FILE_SIZE};
const command_line::arg_descriptor<uint64_t>    argWriteBufferSize = { "db-write-buffer-size", "Size of data base write buffer in megabytes", WRITE_BUFFER_MB_DEFAULT_SIZE};
const command_line::arg_descriptor<uint64_t>    argReadCacheSize = { "db-read-cache-size", "Size of data base read cache in megabytes", READ_BUFFER_MB_DEFAULT_SIZE};
const command_line::arg_descriptor<uint64_t>    argMaxByteLevelSize = { "db-max-byte-level-size", "Size of the database max level base in megabytes", DEFAULT_MAX_BYTES_FOR_LEVEL_BASE};

} //namespace

void DataBaseConfig::initOptions(boost::program_options::options_description& desc) {
  command_line::add_arg(desc, argMaxOpenFiles);
  command_line::add_arg(desc, argMaxFileSize);
  command_line::add_arg(desc, argMaxByteLevelSize);
  command_line::add_arg(desc, argReadCacheSize);
  command_line::add_arg(desc, argWriteBufferSize);
  command_line::add_arg(desc, argBackgroundThreadsCount);
}

DataBaseConfig::DataBaseConfig() :
  dataDir(Tools::getDefaultDataDirectory()),
  backgroundThreadsCount(DEFAULT_BACKGROUND_THREADS_COUNT),
  maxOpenFiles(DEFAULT_MAX_OPEN_FILES),
  maxFileSize(LEVELDB_MAX_FILE_SIZE),
  writeBufferSize(WRITE_BUFFER_MB_DEFAULT_SIZE * MEGABYTE),
  readCacheSize(READ_BUFFER_MB_DEFAULT_SIZE * MEGABYTE),
  maxByteLevelSize(DEFAULT_MAX_BYTES_FOR_LEVEL_BASE * MEGABYTE),
  testnet(false),
  compressionEnabled(true) {
}

bool DataBaseConfig::init(const boost::program_options::variables_map& vm) {
  if (vm.count(argBackgroundThreadsCount.name) != 0 && (!vm[argBackgroundThreadsCount.name].defaulted() || backgroundThreadsCount == 0)) {
    backgroundThreadsCount = command_line::get_arg(vm, argBackgroundThreadsCount);
  }

  if (vm.count(argMaxOpenFiles.name) != 0 && (!vm[argMaxOpenFiles.name].defaulted() || maxOpenFiles == 0)) {
    maxOpenFiles = command_line::get_arg(vm, argMaxOpenFiles);
  }

  if (vm.count(argMaxFileSize.name) != 0 && (!vm[argMaxFileSize.name].defaulted() || maxFileSize == 0)) {
    maxFileSize = command_line::get_arg(vm, argMaxFileSize);
  }
  
  if (vm.count(argWriteBufferSize.name) != 0 && (!vm[argWriteBufferSize.name].defaulted() || writeBufferSize == 0)) {
    writeBufferSize = command_line::get_arg(vm, argWriteBufferSize) *  MEGABYTE;
  }

  if (vm.count(argReadCacheSize.name) != 0 && (!vm[argReadCacheSize.name].defaulted() || readCacheSize == 0)) {
    readCacheSize = command_line::get_arg(vm, argReadCacheSize) * MEGABYTE;
  }

  if (vm.count(argReadCacheSize.name) != 0 && (!vm[argReadCacheSize.name].defaulted() || readCacheSize == 0)) {
    readCacheSize = command_line::get_arg(vm, argReadCacheSize) * MEGABYTE;
  }

  if (vm.count(command_line::arg_data_dir.name) != 0 && (!vm[command_line::arg_data_dir.name].defaulted() || dataDir == Tools::getDefaultDataDirectory())) {
    dataDir = command_line::get_arg(vm, command_line::arg_data_dir);
  }

  configFolderDefaulted = vm[command_line::arg_data_dir.name].defaulted();

  return true;
}

bool DataBaseConfig::isConfigFolderDefaulted() const {
  return configFolderDefaulted;
}

std::string DataBaseConfig::getDataDir() const {
  return dataDir;
}

uint16_t DataBaseConfig::getBackgroundThreadsCount() const {
  return backgroundThreadsCount;
}

uint32_t DataBaseConfig::getMaxOpenFiles() const {
  return maxOpenFiles;
}

uint64_t DataBaseConfig::getMaxFileSize() const {
  return maxFileSize;
}

uint64_t DataBaseConfig::getWriteBufferSize() const {
  return writeBufferSize;
}

uint64_t DataBaseConfig::getReadCacheSize() const {
  return readCacheSize;
}

uint64_t DataBaseConfig::getMaxByteLevelSize() const
{
  return maxByteLevelSize;
}

bool DataBaseConfig::getTestnet() const {
  return testnet;
}

void DataBaseConfig::setConfigFolderDefaulted(bool defaulted) {
  configFolderDefaulted = defaulted;
}

void DataBaseConfig::setDataDir(const std::string& dataDir) {
  this->dataDir = dataDir;
}

void DataBaseConfig::setBackgroundThreadsCount(uint16_t backgroundThreadsCount) {
  this->backgroundThreadsCount = backgroundThreadsCount;
}

void DataBaseConfig::setMaxOpenFiles(uint32_t maxOpenFiles) {
  this->maxOpenFiles = maxOpenFiles;
}

void DataBaseConfig::setMaxFileSize(uint64_t maxFileSize) {
  this->maxFileSize = maxFileSize;
}

void DataBaseConfig::setWriteBufferSize(uint64_t writeBufferSize) {
  this->writeBufferSize = writeBufferSize;
}

void DataBaseConfig::setReadCacheSize(uint64_t readCacheSize) {
  this->readCacheSize = readCacheSize;
}

void DataBaseConfig::setMaxByteLevelSize(uint64_t maxByteLevelSize) {
  this->maxByteLevelSize = maxByteLevelSize;
}

void DataBaseConfig::setTestnet(bool testnet) {
  this->testnet = testnet;
}

bool DataBaseConfig::getCompressionEnabled() const {
  return compressionEnabled;
}

