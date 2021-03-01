// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ElfUtils/ElfFile.h"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/BinaryFormat/ELF.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ELF.h>
#include <llvm/Object/ELFTypes.h>
#include <llvm/Object/SymbolicFile.h>
#include <llvm/Support/CRC.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MathExtras.h>
#include <llvm/Support/MemoryBuffer.h>

#include <type_traits>
#include <utility>
#include <vector>

#include "OrbitBase/File.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/ReadFileToString.h"
#include "OrbitBase/Result.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/ObjectFile.h"
#include "outcome.hpp"
#include "symbol.pb.h"

namespace orbit_elf_utils {

namespace {

using orbit_grpc_protos::LineInfo;
using orbit_grpc_protos::ModuleSymbols;
using orbit_grpc_protos::SymbolInfo;

template <typename ElfT>
class ElfFileImpl : public ElfFile {
 public:
  ElfFileImpl(std::filesystem::path file_path,
              llvm::object::OwningBinary<llvm::object::ObjectFile>&& owning_binary);

  [[nodiscard]] ErrorMessageOr<ModuleSymbols> LoadSymbols() override;
  [[nodiscard]] ErrorMessageOr<uint64_t> GetLoadBias() const override;
  [[nodiscard]] bool HasSymtab() const override;
  [[nodiscard]] bool HasDebugInfo() const override;
  [[nodiscard]] bool HasGnuDebuglink() const override;
  [[nodiscard]] bool Is64Bit() const override;
  [[nodiscard]] std::string GetBuildId() const override;
  [[nodiscard]] std::filesystem::path GetFilePath() const override;
  [[nodiscard]] ErrorMessageOr<LineInfo> GetLineInfo(uint64_t address) override;
  [[nodiscard]] std::optional<GnuDebugLinkInfo> GetGnuDebugLinkInfo() const override;

 private:
  void InitSections();

  const std::filesystem::path file_path_;
  llvm::object::OwningBinary<llvm::object::ObjectFile> owning_binary_;
  llvm::object::ELFObjectFile<ElfT>* object_file_;
  llvm::symbolize::LLVMSymbolizer symbolizer_;
  std::string build_id_;
  bool has_symtab_section_;
  bool has_debug_info_section_;
  std::optional<GnuDebugLinkInfo> gnu_debuglink_info_;
};

template <typename ElfT>
ErrorMessageOr<GnuDebugLinkInfo> ReadGnuDebuglinkSection(
    const typename ElfT::Shdr& section_header, const llvm::object::ELFFile<ElfT>& elf_file) {
  llvm::Expected<llvm::ArrayRef<uint8_t>> contents_or_error =
      elf_file.getSectionContents(&section_header);
  if (!contents_or_error) return ErrorMessage("Could not obtain contents.");

  const llvm::ArrayRef<uint8_t>& contents = contents_or_error.get();

  using ChecksumType = uint32_t;
  constexpr size_t kMinimumPathLength = 1;
  if (contents.size() < kMinimumPathLength + sizeof(ChecksumType)) {
    return ErrorMessage{"Section is too short."};
  }

  constexpr size_t kOneHundredKiB = 100 * 1024;
  if (contents.size() > kOneHundredKiB) {
    return ErrorMessage{"Section is longer than 100KiB. Something is not right."};
  }

  std::string path{};
  path.reserve(contents.size());

  for (const auto& byte : contents) {
    if (byte == '\0') break;
    path.push_back(static_cast<char>(byte));
  }

  if (path.size() > contents.size() - sizeof(ChecksumType)) {
    return ErrorMessage{"No CRC32 checksum found"};
  }

  static_assert(ElfT::TargetEndianness == llvm::support::little,
                "This code only supports little endian architectures.");
  const auto checksum_storage = contents.slice(contents.size() - sizeof(ChecksumType));
  uint32_t reference_crc{};
  std::memcpy(&reference_crc, checksum_storage.data(), sizeof(reference_crc));

  GnuDebugLinkInfo gnu_debuglink_info{};
  gnu_debuglink_info.path = std::filesystem::path{std::move(path)};
  gnu_debuglink_info.crc32_checksum = reference_crc;
  return gnu_debuglink_info;
}

template <typename ElfT>
ElfFileImpl<ElfT>::ElfFileImpl(std::filesystem::path file_path,
                               llvm::object::OwningBinary<llvm::object::ObjectFile>&& owning_binary)
    : file_path_(std::move(file_path)),
      owning_binary_(std::move(owning_binary)),
      has_symtab_section_(false),
      has_debug_info_section_(false) {
  object_file_ = llvm::dyn_cast<llvm::object::ELFObjectFile<ElfT>>(owning_binary_.getBinary());
  InitSections();
}

template <typename ElfT>
void ElfFileImpl<ElfT>::InitSections() {
  const llvm::object::ELFFile<ElfT>* elf_file = object_file_->getELFFile();

  llvm::Expected<typename ElfT::ShdrRange> sections_or_err = elf_file->sections();
  if (!sections_or_err) {
    LOG("Unable to load sections");
    return;
  }

  for (const typename ElfT::Shdr& section : sections_or_err.get()) {
    llvm::Expected<llvm::StringRef> name_or_error = elf_file->getSectionName(&section);
    if (!name_or_error) {
      LOG("Unable to get section name");
      continue;
    }
    llvm::StringRef name = name_or_error.get();

    if (name.str() == ".symtab") {
      has_symtab_section_ = true;
    }

    if (name.str() == ".debug_info") {
      has_debug_info_section_ = true;
    }

    if (name.str() == ".note.gnu.build-id" && section.sh_type == llvm::ELF::SHT_NOTE) {
      llvm::Error error = llvm::Error::success();
      for (const typename ElfT::Note& note : elf_file->notes(section, error)) {
        if (note.getType() != llvm::ELF::NT_GNU_BUILD_ID) continue;

        llvm::ArrayRef<uint8_t> desc = note.getDesc();
        for (const uint8_t& byte : desc) {
          absl::StrAppend(&build_id_, absl::Hex(byte, absl::kZeroPad2));
        }
      }
      if (error) {
        LOG("Error while reading elf notes");
      }
    }

    if (name.str() == ".gnu_debuglink") {
      ErrorMessageOr<GnuDebugLinkInfo> error_or_info = ReadGnuDebuglinkSection(section, *elf_file);
      if (error_or_info.has_value()) {
        gnu_debuglink_info_ = std::move(error_or_info.value());
      } else {
        ERROR("Invalid .gnu_debuglink section in \"%s\". %s", file_path_.string(),
              error_or_info.error().message());
      }
    }
  }
}

template <typename ElfT>
ErrorMessageOr<ModuleSymbols> ElfFileImpl<ElfT>::LoadSymbols() {
  // TODO: if we want to use other sections than .symtab in the future for
  //       example .dynsym, than we have to change this.
  if (!has_symtab_section_) {
    return ErrorMessage("ELF file does not have a .symtab section.");
  }
  bool symbols_added = false;

  OUTCOME_TRY(load_bias, GetLoadBias());

  ModuleSymbols module_symbols;
  module_symbols.set_load_bias(load_bias);
  module_symbols.set_symbols_file_path(file_path_.string());

  for (const llvm::object::ELFSymbolRef& symbol_ref : object_file_->symbols()) {
    if ((symbol_ref.getFlags() & llvm::object::BasicSymbolRef::SF_Undefined) != 0) {
      continue;
    }
    std::string name = symbol_ref.getName() ? symbol_ref.getName().get() : "";
    std::string demangled_name = llvm::demangle(name);

    // Unknown type - skip and generate a warning
    if (!symbol_ref.getType()) {
      LOG("WARNING: Type is not set for symbol \"%s\" in \"%s\", skipping.", name,
          file_path_.string());
      continue;
    }

    // Limit list of symbols to functions. Ignore sections and variables.
    if (symbol_ref.getType().get() != llvm::object::SymbolRef::ST_Function) {
      continue;
    }

    uint64_t symbol_address = symbol_ref.getValue();
    SymbolInfo* symbol_info = module_symbols.add_symbol_infos();
    symbol_info->set_name(name);
    symbol_info->set_demangled_name(demangled_name);
    symbol_info->set_address(symbol_address);
    symbol_info->set_size(symbol_ref.getSize());

    symbols_added = true;
  }
  if (!symbols_added) {
    return ErrorMessage(
        "Unable to load symbols from ELF file, not even a single symbol of "
        "type function found.");
  }
  return module_symbols;
}

template <typename ElfT>
ErrorMessageOr<uint64_t> ElfFileImpl<ElfT>::GetLoadBias() const {
  const llvm::object::ELFFile<ElfT>* elf_file = object_file_->getELFFile();
  llvm::Expected<typename ElfT::PhdrRange> range = elf_file->program_headers();

  if (!range) {
    std::string error =
        absl::StrFormat("Unable to get load bias of ELF file: \"%s\". No program headers found.",
                        file_path_.string());
    ERROR("%s", error);
    return ErrorMessage(std::move(error));
  }

  // Find the executable segment and calculate the load bias based on that segment.
  for (const typename ElfT::Phdr& phdr : range.get()) {
    if (phdr.p_type != llvm::ELF::PT_LOAD) {
      continue;
    }

    if ((phdr.p_flags & llvm::ELF::PF_X) == 0) {
      continue;
    }

    return phdr.p_vaddr - phdr.p_offset;
  }

  std::string error = absl::StrFormat(
      "Unable to get load bias of ELF file: \"%s\". No executable PT_LOAD segment found.",
      file_path_.string());
  ERROR("%s", error);
  return ErrorMessage(std::move(error));
}

template <typename ElfT>
bool ElfFileImpl<ElfT>::HasSymtab() const {
  return has_symtab_section_;
}

template <typename ElfT>
bool ElfFileImpl<ElfT>::HasDebugInfo() const {
  return has_debug_info_section_;
}

template <typename ElfT>
bool ElfFileImpl<ElfT>::HasGnuDebuglink() const {
  return gnu_debuglink_info_.has_value();
}

template <typename ElfT>
std::string ElfFileImpl<ElfT>::GetBuildId() const {
  return build_id_;
}

template <typename ElfT>
std::filesystem::path ElfFileImpl<ElfT>::GetFilePath() const {
  return file_path_;
}

template <typename ElfT>
ErrorMessageOr<LineInfo> orbit_elf_utils::ElfFileImpl<ElfT>::GetLineInfo(uint64_t address) {
  CHECK(has_debug_info_section_);
  auto line_info_or_error = symbolizer_.symbolizeInlinedCode(
      object_file_->getFileName(), {address, llvm::object::SectionedAddress::UndefSection});
  if (!line_info_or_error) {
    return ErrorMessage(absl::StrFormat(
        "Unable to get line number info for \"%s\", address=0x%x: %s", object_file_->getFileName(),
        address, llvm::toString(line_info_or_error.takeError())));
  }

  auto& symbolizer_line_info = line_info_or_error.get();
  const uint32_t number_of_frames = symbolizer_line_info.getNumberOfFrames();

  // Getting back zero frames means there was some kind of problem. We will return a error.
  if (number_of_frames == 0) {
    return ErrorMessage(absl::StrFormat("Unable to get line info for address=0x%x", address));
  }

  for (size_t i = 0; i < number_of_frames; ++i) {
    LOG("  Frame %d: %s:%d", i,
        std::filesystem::path{symbolizer_line_info.getFrame(i).FileName}.filename().string(),
        symbolizer_line_info.getFrame(i).Line);
  }

  const auto& last_frame = symbolizer_line_info.getFrame(number_of_frames - 1);
  if (last_frame.Line == 0) {
    // DWARF Line numbers start counting from 1, and 0 means an error occured.
    return ErrorMessage(absl::StrFormat("Unable to get line info for address=0x%x", address));
  }

  LineInfo line_info;
  line_info.set_source_file(last_frame.FileName);
  line_info.set_source_line(last_frame.Line);
  return line_info;
}

template <>
bool ElfFileImpl<llvm::object::ELF64LE>::Is64Bit() const {
  return true;
}

template <>
bool ElfFileImpl<llvm::object::ELF32LE>::Is64Bit() const {
  return false;
}

template <typename ElfT>
std::optional<GnuDebugLinkInfo> ElfFileImpl<ElfT>::GetGnuDebugLinkInfo() const {
  return gnu_debuglink_info_;
}
}  // namespace

ErrorMessageOr<std::unique_ptr<ElfFile>> ElfFile::CreateFromBuffer(
    const std::filesystem::path& file_path, const void* buf, size_t len) {
  std::unique_ptr<llvm::MemoryBuffer> buffer = llvm::MemoryBuffer::getMemBuffer(
      llvm::StringRef(static_cast<const char*>(buf), len), llvm::StringRef("buffer name"), false);
  llvm::Expected<std::unique_ptr<llvm::object::ObjectFile>> object_file_or_error =
      llvm::object::ObjectFile::createObjectFile(buffer->getMemBufferRef());

  if (!object_file_or_error) {
    return ErrorMessage(absl::StrFormat("Unable to load ELF file \"%s\": %s", file_path.string(),
                                        llvm::toString(object_file_or_error.takeError())));
  }

  return ElfFile::Create(file_path, llvm::object::OwningBinary<llvm::object::ObjectFile>(
                                        std::move(object_file_or_error.get()), std::move(buffer)));
}

ErrorMessageOr<std::unique_ptr<ElfFile>> ElfFile::Create(const std::filesystem::path& file_path) {
  // TODO(hebecker): Remove this explicit construction of StringRef when we
  // switch to LLVM10.
  const std::string file_path_str = file_path.string();
  const llvm::StringRef file_path_llvm{file_path_str};

  llvm::Expected<llvm::object::OwningBinary<llvm::object::ObjectFile>> object_file_or_error =
      llvm::object::ObjectFile::createObjectFile(file_path_llvm);

  if (!object_file_or_error) {
    return ErrorMessage(absl::StrFormat("Unable to load ELF file \"%s\": %s", file_path.string(),
                                        llvm::toString(object_file_or_error.takeError())));
  }

  llvm::object::OwningBinary<llvm::object::ObjectFile>& file = object_file_or_error.get();

  return ElfFile::Create(file_path, std::move(file));
}

ErrorMessageOr<std::unique_ptr<ElfFile>> ElfFile::Create(
    const std::filesystem::path& file_path,
    llvm::object::OwningBinary<llvm::object::ObjectFile>&& file) {
  llvm::object::ObjectFile* object_file = file.getBinary();

  std::unique_ptr<ElfFile> result;

  // Create appropriate ElfFile implementation
  if (llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(object_file) != nullptr) {
    result = std::unique_ptr<ElfFile>(
        new ElfFileImpl<llvm::object::ELF32LE>(file_path, std::move(file)));
  } else if (llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(object_file) != nullptr) {
    result = std::unique_ptr<ElfFile>(
        new ElfFileImpl<llvm::object::ELF64LE>(file_path, std::move(file)));
  } else {
    return ErrorMessage(absl::StrFormat(
        "Unable to load \"%s\": Big-endian architectures are not supported.", file_path.string()));
  }

  return result;
}

ErrorMessageOr<uint32_t> ElfFile::CalculateDebuglinkChecksum(
    const std::filesystem::path& file_path) {
  // TODO(b/180995172): Make this read operation iterative since the potentially read files can be
  // very large (gigabytes).
  ErrorMessageOr<orbit_base::unique_fd> fd_or_error = orbit_base::OpenFileForReading(file_path);

  if (fd_or_error.has_error()) {
    return fd_or_error.error();
  }

  constexpr size_t kBufferSize = 4 * 1024 * 1024;  // 4 MiB
  std::vector<char> buffer(kBufferSize);

  uint32_t rolling_checksum = 0;

  while (true) {
    ErrorMessageOr<size_t> chunksize_or_error =
        orbit_base::ReadFully(fd_or_error.value(), buffer.data(), buffer.size());
    if (chunksize_or_error.has_error()) return chunksize_or_error.error();

    if (chunksize_or_error.value() == 0) break;

    const llvm::StringRef str{buffer.data(), chunksize_or_error.value()};
    rolling_checksum = llvm::crc32(rolling_checksum, str);
  }

  return rolling_checksum;
}

}  // namespace orbit_elf_utils
