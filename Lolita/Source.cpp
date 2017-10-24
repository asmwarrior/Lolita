#include "calc2.h"
#include "config.h"
#include "parsing-bootstrap.h"
#include "parsing-table.h"
#include "debug.h"
#include "parser.h"
#include "arena.hpp"
#include "text/format.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <any>

using namespace std;
using namespace eds;
using namespace eds::loli;
using namespace eds::text;

const ParserBootstrapInfo* info;
AstTraitManager* manager;

class CppEmitter
{
public:
	using CallbackType = std::function<void()>;

	// line
	//
	void EmptyLine()
	{
		buffer_ << std::endl;
	}
	void Comment(std::string_view s)
	{
		WriteIdent();
		buffer_ << "// " << s << std::endl;
	}

	// structure
	//

	void Namespace(std::string_view name, CallbackType cbk)
	{
		WriteStructure("namespace", name, "", false, cbk);
	}
	void Class(std::string_view name, std::string_view parent, CallbackType cbk)
	{
		WriteStructure("class", name, parent, true, cbk);
	}
	void Struct(std::string_view name, std::string_view parent, CallbackType cbk)
	{
		WriteStructure("struct", name, parent, true, cbk);
	}
	void Enum(std::string_view name, std::string_view type, CallbackType cbk)
	{
		WriteStructure("enum", name, type, true, cbk);
	}
	
	
private:
	void WriteIdent()
	{
		for (int i = 0; i < ident_level_; ++i)
			buffer_ << "  ";
	}

	void WriteLine(std::string_view s)
	{
		WriteIdent();
	}

	void WriteStructure(std::string_view type, std::string_view name, std::string_view parent, bool semi, CallbackType cbk)
	{
		// class decl
		WriteIdent();
		buffer_ << type << " " << name;

		if (!parent.empty())
			buffer_ << " : " << parent;

		buffer_ << std::endl;

		// openning brace
		WriteIdent();
		buffer_ << "{\n";

		// body
		ident_level_ += 1;
		cbk();
		ident_level_ -= 1;

		// closing brace
		WriteIdent();
		buffer_ << semi ? "};\n" : "}\n";
	}

	int ident_level_ = 0;
	std::ostringstream buffer_;
};

void G2(const ParserBootstrapInfo& info)
{
	CppEmitter e;

	e.Comment("THIS FILE IS GENERATED BY PROJ. LOLITA.");
	e.Comment("PLEASE DO NOT MODIFY!!!");
	e.Comment("");

	// include <...>

	e.Namespace("eds::loli", [&]() {
		for (const auto& enum_def : info.Enums())
		{
			e.Enum(enum_def.Name(), "", [&]() {
			
			});
		}

		e.EmptyLine();

		for (const auto& base_def : info.Bases())
		{
			e.Struct(base_def.Name(), "public AstObjectBase", [&]() {

			});
		}


	});
}

std::string LoadConfigText()
{
	std::fstream file{ "d:\\aqua.loli.txt" };
	return std::string(std::istreambuf_iterator<char>{file}, {});
}

struct ParsingContext
{
	Arena arena;
	vector<AstItem> items;
};

Token LoadToken(const ParsingTable& table, string_view data, int offset)
{
	auto last_acc_len = 0;
	auto last_acc_category = -1;

	auto state = table.LexerInitialState();
	for (int i = offset; i < data.length(); ++i)
	{
		state = table.LookupDfaTransition(state, data[i]);

		if (!state.IsValid())
		{
			break;
		}
		else if (auto category = table.LookupAccCategory(state); category != -1)
		{
			last_acc_len = i - offset + 1;
			last_acc_category = category;
		}
	}

	if (last_acc_len != 0)
	{
		return Token{ last_acc_category, offset, last_acc_len };
	}
	else
	{
		return Token{ -1, offset, 1 };
	}
}

auto LookupCurrentState(const ParsingTable& table, vector<PdaStateId>& states)
{
	return states.empty()
		? table.ParserInitialState()
		: states.back();
}

bool FeedParser(const ParsingTable& table, vector<PdaStateId>& states, ParsingContext& ctx, int tok)
{
	struct ActionVisitor
	{
		const ParsingTable& table;
		vector<PdaStateId>& states;
		ParsingContext& ctx;
		int tok;

		bool hungry = true;
		bool accepted = false;
		bool error = false;

		ActionVisitor(const ParsingTable& table, vector<PdaStateId>& states, ParsingContext& ctx, int tok)
			: table(table), states(states), ctx(ctx), tok(tok) { }

		void operator()(PdaActionShift action)
		{
			PrintFormatted("Shift state {}\n", action.destination.Value());
			states.push_back(action.destination);
			hungry = false;
		}
		void operator()(PdaActionReduce action)
		{
			auto id = action.production;
			PrintFormatted("Reduce {}\n", debug::ToString_Production(*info, id));

			const auto& production = info->Productions()[id];

			const auto count = production.rhs.size();
			for (auto i = 0; i < count; ++i)
				states.pop_back();

			auto nonterm_id = distance(info->Variables().data(), production.lhs);
			auto src_state = LookupCurrentState(table, states);
			auto target_state = table.LookupGoto(src_state, nonterm_id);
			PrintFormatted("Goto state {}\n", target_state.Value());

			states.push_back(target_state);

			 auto result = production.handle->Invoke(*manager, ctx.arena, AstItemView(ctx.items, count));
			 for (auto i = 0; i < count; ++i)
				 ctx.items.pop_back();
			 ctx.items.push_back(result);
		}
		void operator()(PdaActionError action)
		{
			hungry = false;
			error = true;
		}
	} visitor{ table, states, ctx, tok };

	while (visitor.hungry)
	{
		auto cur_state = LookupCurrentState(table, states);
		// DEBUG:
		if (!cur_state.IsValid()) return visitor.accepted;

		auto action = (tok != -1)
			? table.LookupAction(cur_state, tok)
			: table.LookupActionOnEof(cur_state);

		visit(visitor, action);
	}

	// if (visitor.error) throw 0;

	return visitor.accepted;
}

auto Parse(ParsingContext& ctx, const ParsingTable& table, string_view data)
{
	int offset = 0;
	vector<PdaStateId> states;

	// tokenize and feed parser while not exhausted
	while (offset < data.length())
	{
		auto tok = LoadToken(table, data, offset);

		// update offset
		offset = tok.offset + tok.length;

		// throw for invalid token
		if (tok.category == -1) throw 0;
		// ignore categories in blacklist
		if (tok.category >= table.TerminalCount()) continue;

		PrintFormatted("Lookahead {}\n", debug::ToString_Token(*info, tok.category));
		FeedParser(table, states, ctx, tok.category);
		ctx.items.push_back(AstItem::Create<Token>(tok));
	}

	// finalize parsing
	FeedParser(table, states, ctx, -1);
}

void foo()
{
	auto s = LoadConfigText();

	auto config = config::ParseConfig(s.c_str());
	auto parsing_info = BootstrapParser(*config);
	auto parsing_table = CreateParsingTable(*parsing_info);

	// std::cout << GenerateDataBinding(*parsing_info);
	auto trait_manager = GetAstTraitManager();

	info = parsing_info.get();
	manager = trait_manager.get();
	ParsingContext ctx;

	printf("\n\n");
	auto data = "while(true) { let x=41; break; }";
	Parse(ctx, *parsing_table, data);
	auto exp = ctx.items.front().Extract<Statement*>();
}

int main()
{
	foo();

	// cout << endl << GenerateDataBind(*parsing_info);
	system("pause");
}