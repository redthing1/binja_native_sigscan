#include <cinttypes>
#include <iomanip>
#include "binaryninjaapi.h"

using namespace BinaryNinja;

#define WILD_BYTE -1 // used to identify wild byte when searching for a signature

// converts string signature to array of bytes
// works only with NORM signature type
std::vector<int> parse_hex_string(const std::string& input)
{
	std::vector<int> output;

	std::istringstream iss(input);
	std::string token;
	while (std::getline(iss, token, ' '))
	{
		if (token == "?" || token == "??")
		{
			output.push_back(WILD_BYTE);
		}
		else
		{
			int value = std::stoi(token, nullptr, 16);
			output.push_back(value);
		}
	}

	return output;
}

// converts single instruction to a signature, determines whether each byte should preserved as is or replaced with a wild byte
// enhanced & fixed version of community binja python sigmaker plugin, for more information check readme
void instruction_to_sig(BinaryView* bv, uint64_t addr, size_t inst_length, std::vector<BNConstantReference> consts,
	std::stringstream& sigStream)
{
	auto br = BinaryReader(bv);
	br.Seek(addr);

	if (consts.empty())
	{
		while (inst_length--)
		{
			sigStream
				<< std::hex << std::uppercase << std::setw(2) << std::setfill('0')
				<< static_cast<unsigned int>(br.Read8()) << " ";
		}
	}
	else
	{
		int new_delta = 0;
		for (const auto& cur_const : consts)
		{
			if (cur_const.pointer)
			{
				new_delta += 4;
			}
			else
			{
				int32_t four_bytes;
				bv->Read(&four_bytes, addr + inst_length - (new_delta + 4), sizeof(int32_t));
				if (cur_const.value == four_bytes)
				{
					new_delta += 4;
				}
				else
				{
					int8_t one_byte;
					bv->Read(&one_byte, addr + inst_length - (new_delta + 1), sizeof(int8_t));
					if (cur_const.value == one_byte)
					{
						new_delta += 1;
					}
				}
			}
		}

		br.Seek(addr);
		for (size_t x = 0; x < inst_length - new_delta; ++x)
		{
			sigStream
				<< std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (unsigned int)br.Read8() << " ";
		}
		for (int x = 0; x < new_delta; ++x)
		{
			sigStream << "? ";
		}
	}
}

std::string get_clipboard_text()
{
	if (!::OpenClipboard(nullptr))
	{
		LogError("Failed to open clipboard");
		return std::string {};
	}

	HANDLE hData = ::GetClipboardData(CF_TEXT);
	if (!hData)
	{
		::CloseClipboard();
		LogError("Failed to get clipboard data");
		return std::string {};
	}

	char* pszText = static_cast<char*>(::GlobalLock(hData));
	if (!pszText)
	{
		::GlobalUnlock(hData);
		::CloseClipboard();
		LogError("Failed to extract clipboard data");
		return std::string {};
	}

	std::string clipboard_data(pszText);

	::GlobalUnlock(hData);

	::CloseClipboard();
	return clipboard_data;
}

bool set_clipboard_text(const std::string& text)
{
	if (!::OpenClipboard(nullptr))
	{
		LogError("Failed to open clipboard");
		return false;
	}

	if (!::EmptyClipboard())
	{
		::CloseClipboard();
		LogError("Failed to empty clipboard");
		return false;
	}

	HGLOBAL hData = ::GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(char));
	if (!hData)
	{
		::CloseClipboard();
		LogError("Failed to allocate memory for clipboard data");
		return false;
	}

	char* pszText = static_cast<char*>(::GlobalLock(hData));
	if (!pszText)
	{
		::GlobalFree(hData);
		::CloseClipboard();
		LogError("Failed to lock memory for clipboard data");
		return false;
	}

	std::strcpy(pszText, text.c_str());

	::GlobalUnlock(hData);

	if (!::SetClipboardData(CF_TEXT, hData))
	{
		::GlobalFree(hData);
		::CloseClipboard();
		LogError("Failed to set clipboard data");
		return false;
	}

	::CloseClipboard();
	return true;
}

enum sig_types
{
	NORM,
	CODE
};

void create_sig(BinaryView* view, uint64_t start, uint64_t length, sig_types type)
{
	std::string pattern;
	std::stringstream sig_stream;

	const auto func = view->GetAnalysisFunctionsContainingAddress(start)[0];
	while (length > 0)
	{
		const auto consts = func->GetConstantsReferencedByInstruction(func->GetArchitecture(), start);
		const auto inst_length = view->GetInstructionLength(func->GetArchitecture(), start);

		instruction_to_sig(view, start, inst_length, consts, sig_stream);

		start += inst_length;
		length -= inst_length;
	}

	pattern = std::string {sig_stream.str().substr(0, sig_stream.str().size() - 1)};

	if (type == CODE)
	{
		// variables
		std::string token, mask;

		// create mask
		while (std::getline(sig_stream, token, ' '))
		{
			mask.push_back(token == "?" ? '?' : 'x');
		}

		// create pattern
		std::size_t pos = 0;
		while ((pos = pattern.find(' ', pos)) != std::string::npos)
		{
			pattern.replace(pos, 1, "\\x");
			pos += 2;
		}
		pos = 0;
		while ((pos = pattern.find('?', pos)) != std::string::npos)
		{
			pattern.replace(pos, 1, "00");
			pos += 2;
		}
		pattern = "\"\\x" + pattern + "\", \"" + mask + "\"";
	}

	Log(InfoLog, "%s", pattern.c_str());
	if (!set_clipboard_text(pattern))
	{
		LogError("Failed to copy sig to clipboard");
	}
}

std::string exctract_sig(std::string str, sig_types type)
{
	std::string sig;
	if (type == NORM)
	{
		// should work on stuff like:
		// "48 89 5c 24 08 ? 9a
		// 48 89 5C 24 08 ?? 9A'
		bool have_byte = false;
		for (auto& c : str)
		{
			if (have_byte && c == ' ')
			{
				sig += " ";
				have_byte = false;
				continue;
			}
			else if (!have_byte)
			{
				if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && (c < '0' || c > '9') && c != '?')
				{
					continue;
				}
				else if (c == '?')
				{
					sig += "?";
					have_byte = true;
				}
				else
				{
					if (!sig.empty() && sig.back() != ' ')
					{
						have_byte = true;
					}
					sig += c;
				}
			}
		}
	}
	else
	{
		// should work on stuff like:
		// "\x48\x89\x5c\x24\x08\x00\x9a", "xxxxx?x"
		// \x48\x89\x5C\x24\x08\x00\x9A", "xxxxx?x"'

		// PATTERN
		// find first occurrence of "\\x" in str
		size_t pos = str.find("\\x");
		// keep reading two characters after each "\\x"
		while (pos != std::string::npos)
		{
			// check if there are at least two characters after "\\x"
			if (pos + 2 < str.size() - 1 && str[pos] == '\\' && str[pos + 1] == 'x')
			{
				// read the next two characters
				std::string hex_str = str.substr(pos + 2, 2);
				// append to the sig string
				sig += hex_str + " ";
				// move to next possible byte in pattern
				pos += 4;
			}
			else
			{
				break;
			}
		}

		// MASK
		// find the first occurrence of ',' after pos in str
		pos = str.find(',', pos);
		// read characters until is 'x' or '?'
		while (pos < str.size() && str[pos] != 'x' && str[pos] != '?')
		{
			++pos;
		}
		// read characters until the end of the string or a character that is not 'x' or '?'
		for (size_t i = pos, j = 0; i < str.size() && j * 3 + 2 < sig.size(); ++i, ++j)
		{
			char c = str[i];
			if (c == '?')
			{
				sig[j * 3] = '?';
				sig[j * 3 + 1] = '?';
			}
			else if (c != 'x')
			{
				break;
			}
		}
		sig.pop_back();
	}
	return sig;
}

void find_sig(BinaryView* view, sig_types type)
{
	const std::string clipboard_data = get_clipboard_text();
	if (clipboard_data.empty())
	{
		Log(ErrorLog, "CLIPBOARD DOES NOT CONTAIN ANY TEXT");
		return;
	}
	// Log(InfoLog, "clipboard_data: %s", clipboard_data.c_str());

	const std::string sig = exctract_sig(clipboard_data, type);
	if (sig.empty())
	{
		Log(ErrorLog, "CLIPBOARD DOES NOT CONTAIN VALID SIG");
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
		for (uint64_t i = scan_start; i < bin_end && j < target_bytes.size(); ++i)
		{
			view->Read(&byte, i, 1);
			// Log(InfoLog, "i = 0x%x", byte);
			if (target_bytes[j] == byte || target_bytes[j] == WILD_BYTE)
			{
				if (!found_first)
				{
					found_first = true;
					found_start = i;
				}
				++j;
			}
			else if (found_first)
			{
				found_first = false;
				j = 0;
				i = found_start;
			}
		}
		return found_start;
	};

	Log(InfoLog, "-- NATIVE SIGSCAN START --");
	uint64_t scan_start = bin_start;
	while (true)
	{
		uint64_t j = 0;
		uint64_t found_at = finder(j, scan_start);
		if (j >= target_bytes.size())
		{
			Log(InfoLog, "FOUND SIG AT 0x%llx", found_at);
			scan_start = found_at + 1;
		}
		else
		{
			break;
		}
	}
	Log(InfoLog, "-- NATIVE SIGSCAN END --");
}

extern "C"
{
	BN_DECLARE_CORE_ABI_VERSION

	BINARYNINJAPLUGIN bool CorePluginInit()
	{
		/*PluginCommand::RegisterForRange("NATIVE SIG SCAN TEST", "JUST A TEST",
		    [](BinaryView* view, uint64_t start, uint64_t length) {
		        Log(InfoLog, "------------------");
		        auto func = view->GetAnalysisFunctionsContainingAddress(start)[0];
		        auto const_ref = func->GetConstantsReferencedByInstruction(func->GetArchitecture(), start);
		        for (auto& x : const_ref)
		        {
		            Log(InfoLog, "const_ref: %llx", x.value);
		        }

		        BinaryNinja::ReferenceSource curr {func, func->GetArchitecture(), start};
		        auto callees = view->GetCallees(curr);
		        for (auto& x : callees)
		        {
		            Log(InfoLog, "callee: %llx", x);
		        }

		        auto cref = view->GetCodeReferences(start);
		        for (auto& x : cref)
		        {
		            Log(InfoLog, "[CALL FROM OUTSIDE TO HERE] cref: %llx", x.addr);
		        }
		        auto dref = view->GetDataReferences(start);
		        for (auto& x : cref)
		        {
		            Log(InfoLog, "[CALL FROM OUTSIDE TO HERE] dref: %llx", x.addr);
		        }

		        auto cref1 = view->GetCodeReferencesFrom(curr);
		        for (auto& x : cref1)
		        {
		            Log(InfoLog, "[CALL FROM HERE TO OUTSIDE] cref: %llx", x);
		        }
		        auto dref1 = view->GetDataReferencesFrom(start);
		        for (auto& x : dref1)
		        {
		            Log(InfoLog, "[CALL FROM HERE TO OUTSIDE] dref: %llx", x);
		        }

		        size_t inst_len = view->GetInstructionLength(func->GetArchitecture(), start);
		        Log(InfoLog, "inst_len: %d", inst_len);
		    });*/
		PluginCommand::RegisterForRange("Native SigScan\\Create NORM sig from range", "Create SIGNATURE IN FORMAT '49 28 15 ? ? 30'.",
			[](BinaryView* view, uint64_t start, uint64_t length) { create_sig(view, start, length, NORM); });
		PluginCommand::RegisterForRange("Native SigScan\\Create CODE sig from range",
			"Create SIGNATURE IN FORMAT '\"\\x49\\x28\\x15\\x00\\x00\\x30\", \"xxx??x\"'.",
			[](BinaryView* view, uint64_t start, uint64_t length) { create_sig(view, start, length, CODE); });
		PluginCommand::Register("Native SigScan\\Find NORM sig from clipboard",
			"Find SIGNATURE in current binary (FORMAT '49 28 15 ? ? 30').",
			[](BinaryView* view) { find_sig(view, NORM); });
		PluginCommand::Register("Native SigScan\\Find CODE sig from clipboard",
			"Find SIGNATURE in current binary (FORMAT '\"\\x49\\x28\\x15\\x00\\x00\\x30\", \"xxx??x\"').",
			[](BinaryView* view) { find_sig(view, CODE); });

		Log(InfoLog, "BINJA NATIVE SIGSCAN LOADED");
		return true;
	}
}
