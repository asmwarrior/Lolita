#include "core/config.h"
#include "core/errors.h"
#include "text/format.h"
#include "text/text-utils.h"

using namespace std;
using namespace eds::text;

namespace eds::loli::config
{
    struct ConfigParsingError : runtime_error
    {
        const char* pos;

    public:
        ConfigParsingError(const char* pos, const string& msg)
            : runtime_error(msg), pos(pos) {}
    };

    void SkipWhitespace(zstring& s, bool toggle = true)
    {
        if (!toggle)
            return;

        bool halt = false;
        while (!halt)
        {
            halt = true;

            if (*s == '#')
            {
                while (*s && *s != '\n')
                    Consume(s);

                halt = false;
            }

            while (ConsumeIfAny(s, " \r\n\t"))
            {
                halt = false;
            }
        }
    }

    bool TryParseConstant(zstring& s, const char* text, bool skip_ws = true)
    {
        SkipWhitespace(s, skip_ws);
        return ConsumeIfSeq(s, text);
    }

    void ParseConstant(zstring& s, const char* text, bool skip_ws = true)
    {
        SkipWhitespace(s, skip_ws);

        // expecting constant
        if (!ConsumeIfSeq(s, text))
        {
            throw ConfigParsingError{s, Format("expecting {}", text)};
        }
    }

    string ParseIdentifier(zstring& s, bool skip_ws = true)
    {
        SkipWhitespace(s, skip_ws);

        // expecting letter
        if (!isalpha(*s))
        {
            throw ConfigParsingError{s, "expecting <identifier>"};
        }

        string buf;
        while (isalnum(*s) || *s == '_')
        {
            buf.push_back(Consume(s));
        }

        // TODO: exclude keywords
        return buf;
    }

    string ParseString(zstring& s, bool skip_ws = true)
    {
        SkipWhitespace(s, skip_ws);

        // expecting string
        if (!ConsumeIf(s, '"'))
        {
            throw ConfigParsingError{s, "expecting <string>"};
        }

        string buf{'"'};
        while (*s)
        {
            if (ConsumeIf(s, '"'))
            {
                if (ConsumeIf(s, '"'))
                {
                    buf.push_back('"');
                }
                else
                {
                    buf.push_back('"');
                    return buf;
                }
            }
            else
            {
                buf.push_back(Consume(s));
            }
        }

        // unexpected eof
        throw ConfigParsingError{s, "unexpected <eof>"};
    }

    QualType ParseTypeSpec(zstring& s)
    {
        auto type = ParseIdentifier(s);
        auto qual = ""s;
        if (TryParseConstant(s, "'", false))
        {
            qual = ParseIdentifier(s, false);
        }

        return QualType{move(type), move(qual)};
    }

    void ParseTokenDefinition(ParsingConfiguration& config, zstring& s, bool ignore)
    {
        auto name = ParseIdentifier(s);
        ParseConstant(s, "=");
        auto regex = ParseString(s);
        ParseConstant(s, ";");

        auto& target_vec = ignore ? config.ignored_tokens : config.tokens;
        target_vec.push_back(
            TokenDefinition{move(name), move(regex)});
    }

    void ParseEnumDefinition(ParsingConfiguration& config, zstring& s)
    {
        auto name = ParseIdentifier(s);
        ParseConstant(s, "{");

        vector<string> values;
        while (!TryParseConstant(s, "}"))
        {
            values.push_back(ParseIdentifier(s));
            ParseConstant(s, ";");
        }

        config.enums.push_back(
            EnumDefinition{move(name), move(values)});
    }

    void ParseBaseDefinition(ParsingConfiguration& config, zstring& s)
    {
        auto name = ParseIdentifier(s);
        ParseConstant(s, ";");

        config.bases.push_back(
            BaseDefinition{move(name)});
    }

    void ParseNodeDefinition(ParsingConfiguration& config, zstring& s)
    {
        auto name   = ParseIdentifier(s);
        auto parent = ""s;
        if (TryParseConstant(s, ":"))
        {
            parent = ParseIdentifier(s);
        }

        ParseConstant(s, "{");

        vector<NodeMember> members;
        while (!TryParseConstant(s, "}"))
        {
            auto type = ParseTypeSpec(s);

            auto field_name = ParseIdentifier(s);
            ParseConstant(s, ";");

            members.push_back(
                NodeMember{type, field_name});
        }

        config.nodes.push_back(
            NodeDefinition{move(name), move(parent), move(members)});
    }

    void ParseRuleDefinition(ParsingConfiguration& config, zstring& s)
    {
        auto name = ParseIdentifier(s);
        ParseConstant(s, ":");
        auto type = ParseTypeSpec(s);

        vector<RuleItem> items;
        while (true)
        {
            ParseConstant(s, "=");

            vector<RuleSymbol> rhs;

            SkipWhitespace(s);
            while (isalpha(*s) || *s == '"')
            {
                auto symbol = ""s;
                if (*s == '"')
                {
                    symbol = ParseString(s);
                }
                else
                {
                    symbol = ParseIdentifier(s);
                }

                auto assign = ""s;
                if (TryParseConstant(s, "!"))
                {
                    assign = "!";
                }
                else if (TryParseConstant(s, "&"))
                {
                    assign = "&";
                }
                else if (TryParseConstant(s, ":"))
                {
                    assign = ParseIdentifier(s);
                }

                rhs.push_back(
                    RuleSymbol{symbol, assign});

                // TODO: make this look better
                SkipWhitespace(s);
            }

            optional<QualType> klass_hint;
            if (TryParseConstant(s, "->"))
            {
                if (TryParseConstant(s, "_"))
                {
                    klass_hint = QualType{"_", ""};
                }
                else
                {
                    klass_hint = ParseTypeSpec(s);
                }
            }

            items.push_back(
                RuleItem{move(rhs), move(klass_hint)});

            if (TryParseConstant(s, ";"))
                break;
        }

        config.rules.push_back(
            RuleDefinition{move(type), move(name), move(items)});
    }

    void ParseConfigInternal(ParsingConfiguration& config, zstring& s)
    {
        while (*s)
        {
            if (TryParseConstant(s, "token"))
            {
                ParseTokenDefinition(config, s, false);
            }
            else if (TryParseConstant(s, "ignore"))
            {
                ParseTokenDefinition(config, s, true);
            }
            else if (TryParseConstant(s, "enum"))
            {
                ParseEnumDefinition(config, s);
            }
            else if (TryParseConstant(s, "base"))
            {
                ParseBaseDefinition(config, s);
            }
            else if (TryParseConstant(s, "node"))
            {
                ParseNodeDefinition(config, s);
            }
            else if (TryParseConstant(s, "rule"))
            {
                ParseRuleDefinition(config, s);
            }
            else
            {
                throw ConfigParsingError{s, "unexpected token"};
            }

            SkipWhitespace(s);
        }
    }

    unique_ptr<ParsingConfiguration> ParseConfig(const string& data)
    {
        auto result = make_unique<ParsingConfiguration>();

        try
        {
            auto p = data.c_str();
            ParseConfigInternal(*result, p);
        }
        catch (const ConfigParsingError& err)
        {
            auto around_text = string{err.pos}.substr(0, 20);
            throw ParserConstructionError{
                Format("LoadConfig: Failed parsing config file:{} at around \"{}\".", err.what(), around_text)};
        }

        return result;
    }
}