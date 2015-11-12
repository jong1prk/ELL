/// CommandLineParser.cpp

#include "CommandLineParser.h"

#include <iostream>
using std::endl;
using std::cerr;

#include <algorithm>
using std::min;
using std::max;

namespace utilities
{
	//
	// OptionInfo class
	//
	OptionInfo::OptionInfo(string name, string short_name, string description, string default_value, function<bool(string)> set_value_callback) : name(name), short_name(short_name), description(description), default_value_string(default_value), set_value_callbacks({ set_value_callback })
	{
	}

	//
	// CommandLineParser class
	//
	CommandLineParser::CommandLineParser(int argc, char**argv)
	{
		SetArgs(argc, argv);
	}

	void CommandLineParser::SetArgs(int argc, char** argv)
	{
		_original_args.insert(_original_args.end(), &argv[0], &argv[argc]);
	}

	void CommandLineParser::ParseArgs()
	{
		// TODO: should probably throw an exception here
		if (_original_args.size() == 0)
			return;

		string exe_path = _original_args[0];
		size_t slash_pos = exe_path.find_last_of("/\\");
		if (slash_pos == string::npos)
		{
			_exe_name = exe_path;
		}
		else
		{
			_exe_name = exe_path.substr(slash_pos + 1);
		}

		// While we're parsing the arguments, we may Add new conditional options. If we do so, we need
		// to reparse the inputs in case some earlier commandline option referred to one of these new
		// options. So we repeatedly parse the command line text until we haven't added any new conditional options.
	
		bool needs_reparse = true;
		while (needs_reparse)
		{
			set<string> unset_args;
			for (const auto& opt : _options)
			{
				unset_args.insert(opt.first);
			}

			needs_reparse = false;
			size_t argc = _original_args.size();
			for (int index = 1; index < argc; index++)
			{
				string arg = _original_args[index];
				if (arg[0] == '-') // it's an option
				{
					string option;
					if (arg[1] == '-') // long name
					{
						option = string(arg.begin() + 2, arg.end());
					}
					else // short name
					{
						string short_name = string(arg.begin() + 1, arg.end());
						option = _short_to_long_name_map[short_name];
					}

					if (option == "" && arg != "--") // "--" is the special "ignore this" option --- used to put between flag arguments and the filepath
					{
						cerr << "Error: unknown option " << arg << ", skipping." << endl;
						if (index < argc - 1 && _original_args[index + 1][0] != '-')  // skip the Next value as well, unless it's an option
						{
							index++;
						}
					}

					if (option != "")
					{
						unset_args.erase(option);
						if (index < argc - 1)
						{
							string val = _original_args[index + 1];
							if (val[0] == '-')
							{
								needs_reparse = SetOption(option, "true") || needs_reparse;
							}
							else
							{
								needs_reparse = SetOption(option, val) || needs_reparse;
								index++;
							}
						}
						else // this is the last thing on the line --- assume it's a shortcut for --option true
						{
							needs_reparse = SetOption(option, "true") || needs_reparse;
						}
					}
				}
				else
				{
					_args.push_back(_original_args[index]);
				}
			}

			// Need to set default args here, in case one of them enables a conditional argument set
			needs_reparse = SetDefaultArgs(unset_args) || needs_reparse;
		}
	}

	bool CommandLineParser::SetDefaultArgs(const set<string>& unset_args)
	{
		bool needs_reparse = false;
		for (string arg_name : unset_args)
		{
			// look up arg
			auto iter = _options.find(arg_name);
			if (iter != _options.end())
			{
				const OptionInfo& arg_info = iter->second;
				string default_val = arg_info.default_value_string;
				needs_reparse = SetOption(arg_name, default_val) || needs_reparse;
			}
		}
		return needs_reparse;
	}

	size_t CommandLineParser::NumArgs()
	{
		return _args.size();
	}

	string CommandLineParser::GetArg(int index)
	{
		return _args[index];
	}

	bool CommandLineParser::HasOption(string option)
	{
		return _options.find(option) != _options.end();
	}

	void CommandLineParser::AddOption(const OptionInfo& info)
	{
		_options[info.name] = info;
		_doc_entries.emplace_back(DocumentationEntry::type::option, info.name);

		if (info.short_name != "")
		{
			_short_to_long_name_map[info.short_name] = info.name;
		}
	}

	inline bool find_best_match(string str, const vector<string>& val_names, string& result_string)
	{
		bool did_find_one = false;
		for (const auto& val_name : val_names)
		{
			if (val_name.find(str) != string::npos)
			{
				if (did_find_one)
				{
					return false;
				}

				result_string = val_name;
				did_find_one = true;
			}
		}

		return did_find_one;
	}

	bool CommandLineParser::SetOption(string option_name, string option_val)
	{
		string old_value_string = _options[option_name].current_value_string;
		_options[option_name].current_value_string = option_val;

		bool ok = true;
		bool did_enable_more_params = false;
		for (auto set_value_cb : _options[option_name].set_value_callbacks)
		{
			ok = ok && set_value_cb(option_val);
		}

		if (ok)
		{
			auto iter = _options[option_name].did_set_value_callbacks.begin();
			while (iter != _options[option_name].did_set_value_callbacks.end())
			{
				auto& did_set_value_cb = *iter;
				bool did_set = did_set_value_cb(_options[option_name].current_value_string);
				if (did_set)
				{
					did_enable_more_params = true;
					// remove from list once it's called
					iter = _options[option_name].did_set_value_callbacks.erase(iter);
				}
				else
				{
					++iter;
				}
			}

		}
		else
		{
			_options[option_name].current_value_string = old_value_string;
		}

		return did_enable_more_params;
	}

	void CommandLineParser::AddDocumentationString(string str)
	{
		_doc_entries.emplace_back(DocumentationEntry::type::str, str);
	}

	string option_name_string(const OptionInfo& option)
	{
		if (option.short_name == "")
		{
			return option.name + " [" + option.default_value_string + "]";
		}
		else
		{
			return option.name + " (-" + option.short_name + ") [" + option.default_value_string + "]";
		}
	}

	size_t option_name_help_length(const OptionInfo& option)
	{
		size_t len = option.name.size() + 2;
		if (option.short_name != "")
		{
			len += (option.short_name.size() + 4);
		}
	
		len += option.default_value_string.size() + 3; // 3 for " [" + "]" at begin/end

		const size_t max_name_len = 32;
		return min(max_name_len, len);
	}

	void CommandLineParser::PrintUsage(ostream& out)
	{
		// Find longest option name so we can align descriptions
		size_t longest_name = 0;
		for (const auto& iter : _options)
		{
			if (iter.first == iter.second.name) // wasn't a previously-undefined option
			{
				longest_name = max(longest_name, option_name_help_length(iter.second));
			}
		}

		out << "Usage: " << _exe_name << " [options]" << endl;
		out << endl;

		for (const auto& entry: _doc_entries)
		{
			switch (entry.EntryType)
			{
			case DocumentationEntry::type::option:
			{
				const auto& info = _options[entry.EntryString];
				string option_name = option_name_string(info);
				size_t this_option_name_len = option_name_help_length(info);
				size_t pad_len = 2 + (longest_name - this_option_name_len);
				string padding(pad_len, ' ');
				out << "\t--" << option_name << padding << info.description;
				if (info.enum_values.size() > 0)
				{
					out << "  {";
					string sep = "";
					out << info.enum_values[0];
					for (int index = 1; index < info.enum_values.size(); ++index)
					{
						out << " | " << info.enum_values[index];
					}
					out << "}";
				}
				out << endl;
			}
			break;

			case DocumentationEntry::type::str:
				out << entry.EntryString << endl;
				break;
			}
		}
	}

	void CommandLineParser::PrintCurrentValues(ostream& out)
	{
		out << "Current parameters for " << _exe_name << endl;

		set<string> visited_options;
		for (auto& entry: _doc_entries)
		{
			if (entry.EntryType == DocumentationEntry::type::option)
			{
				const auto& opt = _options[entry.EntryString];
				visited_options.insert(opt.name);
				out << "\t--" << opt.name << ": ";
				if (opt.current_value_string != "")
				{
					out << opt.current_value_string;
					if (opt.current_value_string == opt.default_value_string)
					{
						out << " (default)";
					}
					out << endl;
				}
				else
				{
					out << "[" << opt.default_value_string << "]" << endl;
				}
			}
		}

		bool did_print = false;
		for (auto& opt : _options)
		{
			if (visited_options.find(opt.first) == visited_options.end())
			{
				if (!did_print)
				{
					out << endl;
					out << "Unknown parameters" << endl;
				}
				out << "\t--" << opt.first << ": " << opt.second.current_value_string << endl;
				did_print = true;
			}
		}
	}
}