#include <cinttypes>
#include <iomanip>

#include "binaryninjaapi.h"

using namespace BinaryNinja;

#define PLUGIN_NAME "Native SigScan"
#define PLUGIN_ID "nativeSigScan"

#define WILD_BYTE -1 // used to identify wild byte when searching for a signature

// converts string signature to array of bytes
// works only with NORM signature type
std::vector<int> parse_hex_string(const std::string& input) {
  std::vector<int> output;

  std::istringstream iss(input);
  std::string token;
  while (std::getline(iss, token, ' ')) {
    if (token == "?" || token == "??") {
      output.push_back(WILD_BYTE);
    } else {
      int value = std::stoi(token, nullptr, 16);
      output.push_back(value);
    }
  }

  return output;
}

// converts single instruction to a signature, determines whether each byte should preserved as is or replaced with a
// wild byte enhanced & fixed version of community binja python sigmaker plugin, for more information check readme
void instruction_to_signature(
    BinaryView* bv, uint64_t addr, size_t inst_length, std::vector<BNConstantReference> consts,
    std::stringstream& sig_stream, bool allow_custom_wildcard
) {
  // determine the wildcard character to use
  const std::string wildcard =
      allow_custom_wildcard ? Settings::Instance()->Get<std::string>(PLUGIN_ID ".normSigCustomWildcard") : "?";

  // create a binary reader and seek to the address of the instruction
  auto br = BinaryReader(bv);
  br.Seek(addr);

  // if there are no constant references, simply output the entire instruction as hex bytes
  if (consts.empty()) {
    while (inst_length--) {
      // read each byte and output as a hex string
      sig_stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(br.Read8()) << " ";
    }
  } else {
    // new_delta: how many bytes at the end should be wildcards
    int new_delta = 0;

    // iterate through each constant reference
    for (const auto& cur_const : consts) {
      if (cur_const.pointer) {
        // if it's a pointer, add 4 to new_delta (assuming 32-bit pointers)
        new_delta += 4;
      } else {
        // if it's a value, first check if it matches the last 4 bytes
        int32_t four_bytes;
        // read 4 bytes from the end of the instruction, offset by current new_delta
        bv->Read(&four_bytes, addr + inst_length - (new_delta + 4), sizeof(int32_t));
        if (cur_const.value == four_bytes) {
          // if it matches, add 4 to new_delta
          new_delta += 4;
        } else {
          // if 4-byte match fails, check if it matches the last byte
          int8_t one_byte;
          // read 1 byte from the end of the instruction, offset by current new_delta
          bv->Read(&one_byte, addr + inst_length - (new_delta + 1), sizeof(int8_t));
          if (cur_const.value == one_byte) {
            // if it matches, add 1 to new_delta
            new_delta += 1;
          }
        }
      }
    }

    // ensure new_delta is valid
    if (new_delta > inst_length) {
      new_delta = inst_length;
      auto log = bv->CreateLogger(PLUGIN_NAME);
      std::stringstream ss;
      ss << "invalid new_delta value processing instruction @ 0x" << std::hex << addr
         << ", setting to inst_length=" << inst_length;
      log->Log(ErrorLog, ss.str().c_str());
    }

    // seek back to the start of the instruction
    br.Seek(addr);

    // output the non-wildcard part of the instruction as hex bytes
    for (size_t x = 0; x < inst_length - new_delta; ++x) {
      sig_stream << std::hex << std::setw(2) << std::setfill('0') << (unsigned int) br.Read8() << " ";
    }

    // output wildcards for the remaining bytes
    for (int x = 0; x < new_delta; ++x) {
      sig_stream << wildcard << " ";
    }
  }
}

enum sig_types {
  NORM,
  CODE,
};

void create_signature(BinaryView* view, uint64_t start, uint64_t length, sig_types type) {
  auto session_id = view->GetFile()->GetSessionId();
  auto logger = LogRegistry::CreateLogger(PLUGIN_NAME, session_id);

  if (view->GetCurrentView().find("Raw") != std::string::npos ||
      view->GetCurrentView().find("Hex") != std::string::npos) {
    logger->Log(ErrorLog, "CANNOT CREATE SIG FROM RAW OR HEX VIEW");
    return;
  }

  std::string pattern;
  std::stringstream sig_stream;
  bool instruction_parsing = view->GetAnalysisFunctionsContainingAddress(start).size() > 0;

  if (instruction_parsing) {
    auto func = view->GetAnalysisFunctionsContainingAddress(start)[0];
    while (length > 0) {
      const auto consts = func->GetConstantsReferencedByInstruction(func->GetArchitecture(), start);
      const auto inst_length = view->GetInstructionLength(func->GetArchitecture(), start);

      instruction_to_signature(view, start, inst_length, consts, sig_stream, type == NORM);

      start += inst_length;
      length -= inst_length;
    }
  } else {
    instruction_to_signature(view, start, length, {}, sig_stream, type == NORM);
  }

  pattern = std::string{sig_stream.str().substr(0, sig_stream.str().size() - 1)};

  if (type == CODE) {
    // variables
    std::string token, mask;

    // create mask
    while (std::getline(sig_stream, token, ' ')) {
      mask.push_back(token == "?" ? '?' : 'x');
    }

    // create pattern
    std::size_t pos = 0;
    while ((pos = pattern.find(' ', pos)) != std::string::npos) {
      pattern.replace(pos, 1, "\\x");
      pos += 2;
    }
    pos = 0;
    while ((pos = pattern.find('?', pos)) != std::string::npos) {
      pattern.replace(pos, 1, "00");
      pos += 2;
    }
    pattern = "\"\\x" + pattern + "\", \"" + mask + "\"";
  }

  if (!instruction_parsing) {
    pattern += " [RAW BYTES - NO WILDCARDS]";
  }
  logger->Log(instruction_parsing ? InfoLog : WarningLog, "%s", pattern.c_str());
}

void replace_all(std::string& str, const std::string& from, const std::string& to) {
  if (from.empty()) {
    return;
  }
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
}

std::string extract_signature(std::string str, sig_types type, bool scan_for_custom_wildcard) {
  std::string sig;
  if (type == NORM) {
    // replace custom wildcards with question marks
    if (scan_for_custom_wildcard) {
      std::string custom_wildcard = Settings::Instance()->Get<std::string>(PLUGIN_ID ".normSigCustomWildcard");
      replace_all(str, custom_wildcard, "?");
    }

    // should work on stuff like:
    // "48 89 5c 24 08 ? 9a
    // 48 89 5C 24 08 ?? 9A'
    bool have_byte = false;
    int cur_byte_len = 0;
    for (auto& c : str) {
      if (have_byte && c == ' ') {
        if (cur_byte_len > 2) {
          return "";
        }
        sig += " ";
        have_byte = false;
        cur_byte_len = 0;
      } else {
        if ((c < 'a' || c > 'f') && (c < 'A' || c > 'F') && (c < '0' || c > '9') && c != '?') {
          continue;
        } else if (c == '?') {
          sig += "?";
          ++cur_byte_len;
          have_byte = true;
        } else {
          if (!sig.empty() && sig.back() != ' ') {
            have_byte = true;
          }
          sig += c;
          ++cur_byte_len;
        }
      }
    }
  } else {
    // should work on stuff like:
    // "\x48\x89\x5c\x24\x08\x00\x9a", "xxxxx?x"
    // \x48\x89\x5C\x24\x08\x00\x9A", "xxxxx?x"'

    // PATTERN
    // find first occurrence of "\\x" in str
    size_t pos = str.find("\\x");
    // keep reading two characters after each "\\x"
    while (pos != std::string::npos) {
      // check if there are at least two characters after "\\x"
      if (pos + 2 < str.size() - 1 && str[pos] == '\\' && str[pos + 1] == 'x') {
        // read the next two characters
        std::string hex_str = str.substr(pos + 2, 2);
        // append to the sig string
        sig += hex_str + " ";
        // move to next possible byte in pattern
        pos += 4;
      } else {
        break;
      }
    }

    // MASK
    // find the first occurrence of ',' after pos in str
    pos = str.find(',', pos);
    if (pos != std::string::npos) {
      // read characters until is 'x' or '?'
      while (pos < str.size() && str[pos] != 'x' && str[pos] != '?') {
        ++pos;
      }
      // read characters until the end of the string or a character that is not 'x' or '?'
      for (size_t i = pos, j = 0; i < str.size() && j * 3 + 2 < sig.size(); ++i, ++j) {
        char c = str[i];
        if (c == '?') {
          sig[j * 3] = '?';
          sig[j * 3 + 1] = '?';
        } else if (c != 'x') {
          break;
        }
      }
    }
  }
  if (sig.back() == ' ') {
    sig.pop_back();
  }
  return sig;
}

void search_for_signature(BinaryView* view, sig_types type) {
  auto session_id = view->GetFile()->GetSessionId();
  auto logger = LogRegistry::CreateLogger(PLUGIN_NAME, session_id);

  std::string input_data /*= get_clipboard_text()*/;
  if (!GetTextLineInput(input_data, "Enter signature to find", PLUGIN_NAME)) {
    logger->Log(ErrorLog, "FAILED TO GRAB INPUT");
    return;
  }

  if (input_data.empty()) {
    logger->Log(ErrorLog, "INPUT DOES NOT CONTAIN ANY TEXT");
    return;
  }
  // Log(InfoLog, "input_data: %s", input_data.c_str());

  const std::string sig = extract_signature(
      input_data, type,
      type == NORM && Settings::Instance()->Get<bool>(PLUGIN_ID ".inNormSigScanCustomWildcard") &&
          Settings::Instance()->Get<std::string>(PLUGIN_ID ".normSigCustomWildcard") != "?"
  );

  if (sig.empty()) {
    logger->Log(ErrorLog, "INPUT IS NOT VALID SIG");
    return;
  }
  // Log(InfoLog, "sig: %s", sig.c_str());

  std::vector<int> target_bytes = parse_hex_string(sig);

  uint64_t bin_start = view->GetStart();
  uint64_t bin_end = view->GetEnd();
  uint64_t bin_size = bin_end - bin_start;
  // Log(InfoLog, "bin_start = 0x%llx", bin_start);
  // Log(InfoLog, "bin_end = 0x%llx", bin_end);
  // Log(InfoLog, "bin_size = 0x%llx", bin_size);

  auto finder = [&](uint64_t& j, uint64_t scan_start) -> uint64_t {
    unsigned char byte;
    bool found_first = false;
    uint64_t found_start = 0;
    for (uint64_t i = scan_start; i < bin_end && j < target_bytes.size(); ++i) {
      view->Read(&byte, i, 1);
      // Log(InfoLog, "i = 0x%x", byte);
      if (target_bytes[j] == byte || target_bytes[j] == WILD_BYTE) {
        if (!found_first) {
          found_first = true;
          found_start = i;
        }
        ++j;
      } else if (found_first) {
        found_first = false;
        j = 0;
        i = found_start;
      }
    }
    return found_start;
  };

  logger->Log(InfoLog, "-- SIGSCAN FIND START --");
  uint64_t scan_start = bin_start;
  int64_t next_found_at = -1;
  bool next_found = false;
  while (true) {
    uint64_t j = 0;
    uint64_t found_at = finder(j, scan_start);
    if (j >= target_bytes.size()) {
      logger->Log(InfoLog, "FOUND SIG AT 0x%llx", found_at);
      scan_start = found_at + 1;
      if (!next_found) {
        next_found_at = found_at;
        if (next_found_at > view->GetCurrentOffset()) {
          next_found = true;
        }
      }
    } else {
      break;
    }
  }

  if (Settings::Instance()->Get<bool>(PLUGIN_ID ".navigateToNextResultAfterSearch") && next_found_at != -1) {
    view->Navigate(view->GetFile()->GetCurrentView(), next_found_at);
  }

  logger->Log(InfoLog, "-- SIGSCAN FIND END --");
}

extern "C" {
BN_DECLARE_CORE_ABI_VERSION

BINARYNINJAPLUGIN bool CorePluginInit() {
  PluginCommand::RegisterForRange(
      PLUGIN_NAME "\\Create Signature", "Create signature for current selection.",
      [](BinaryView* view, uint64_t start, uint64_t length) { create_signature(view, start, length, NORM); }
  );
  PluginCommand::Register(PLUGIN_NAME "\\Find Signature", "Find signature in current binary.", [](BinaryView* view) {
    search_for_signature(view, NORM);
  });

  auto settings = Settings::Instance();
  settings->RegisterGroup(PLUGIN_ID "", PLUGIN_NAME);
  settings->RegisterSetting(
      PLUGIN_ID ".normSigCustomWildcard",
      R"~({
                        "title": "Custom wildcard",
                        "type": "string",
                        "default": "??",
	                    "description": "Wildcard character(s) used when creating NORM patterns."
	                    })~"
  );
  settings->RegisterSetting(
      PLUGIN_ID ".inNormSigScanCustomWildcard",
      R"~({
                        "title": "Scan for custom wildcard",
                        "type": "boolean",
                        "default": true,
	                    "description": "Option to scan for custom wildcards when finding NORM patterns (only used if default wildcard is changed), ideally should be set to false if custom wildcard can be a regular byte found in disassembly (0x00-0xFF)."
	                    })~"
  );
  settings->RegisterSetting(
      PLUGIN_ID ".navigateToNextResultAfterSearch",
      R"~({
                        "title": "Navigate to the closest result",
                        "type": "boolean",
                        "default": false,
	                    "description": "Option to automatically navigate the current view to the closest result relative to the current offset (goes for the closest greater offset or the closest smaller if no greater found)."
	                    })~"
  );

  Log(InfoLog, PLUGIN_NAME " loaded");
  return true;
}
}
